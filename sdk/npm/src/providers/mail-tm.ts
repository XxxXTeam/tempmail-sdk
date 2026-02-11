import { EmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';

const CHANNEL: Channel = 'mail-tm';
const BASE_URL = 'https://api.mail.tm';

const DEFAULT_HEADERS: Record<string, string> = {
  'Content-Type': 'application/json',
  'Accept': 'application/json',
};

/**
 * 生成随机字符串
 */
function randomString(length: number): string {
  const chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
  let result = '';
  for (let i = 0; i < length; i++) {
    result += chars[Math.floor(Math.random() * chars.length)];
  }
  return result;
}

/**
 * 获取可用域名列表
 * API: GET /domains
 */
async function getDomains(): Promise<string[]> {
  const response = await fetch(`${BASE_URL}/domains`, {
    method: 'GET',
    headers: DEFAULT_HEADERS,
  });

  if (!response.ok) {
    throw new Error(`Failed to get domains: ${response.status}`);
  }

  const data = await response.json();
  /* 兼容两种响应格式：
   * - Accept: application/ld+json → Hydra 格式 { "hydra:member": [...] }
   * - Accept: application/json   → 纯数组 [...]
   */
  const members = Array.isArray(data) ? data : (data['hydra:member'] || []);
  return members
    .filter((d: any) => d.isActive)
    .map((d: any) => d.domain);
}

/**
 * 创建账号
 * API: POST /accounts
 */
async function createAccount(address: string, password: string): Promise<any> {
  const response = await fetch(`${BASE_URL}/accounts`, {
    method: 'POST',
    headers: { ...DEFAULT_HEADERS, 'Content-Type': 'application/ld+json' },
    body: JSON.stringify({ address, password }),
  });

  if (!response.ok) {
    const text = await response.text();
    throw new Error(`Failed to create account: ${response.status} ${text}`);
  }

  return response.json();
}

/**
 * 获取 Bearer Token
 * API: POST /token
 */
async function getToken(address: string, password: string): Promise<string> {
  const response = await fetch(`${BASE_URL}/token`, {
    method: 'POST',
    headers: DEFAULT_HEADERS,
    body: JSON.stringify({ address, password }),
  });

  if (!response.ok) {
    throw new Error(`Failed to get token: ${response.status}`);
  }

  const data = await response.json();
  return data.token;
}

/**
 * 创建临时邮箱
 * 流程: 获取域名 → 生成随机邮箱/密码 → 创建账号 → 获取 Token
 */
export async function generateEmail(): Promise<EmailInfo> {
  // 1. 获取可用域名
  const domains = await getDomains();
  if (domains.length === 0) {
    throw new Error('No available domains');
  }

  // 2. 生成随机邮箱和密码
  const domain = domains[Math.floor(Math.random() * domains.length)];
  const username = randomString(12);
  const address = `${username}@${domain}`;
  const password = randomString(16);

  // 3. 创建账号
  const account = await createAccount(address, password);

  // 4. 获取 Bearer Token
  const token = await getToken(address, password);

  return {
    channel: CHANNEL,
    email: address,
    token: token,
    createdAt: account.createdAt,
  };
}

/**
 * 将 mail.tm 的消息格式扁平化为 normalizeEmail 可处理的格式
 * mail.tm 的 from 是 {name, address} 对象，to 是数组，html 是字符串数组
 */
function flattenMessage(msg: any, recipientEmail: string): any {
  return {
    id: msg.id || '',
    from: msg.from?.address || '',
    to: msg.to?.[0]?.address || recipientEmail,
    subject: msg.subject || '',
    text: msg.text || '',
    html: Array.isArray(msg.html) ? msg.html.join('') : (msg.html || ''),
    createdAt: msg.createdAt || '',
    seen: msg.seen ?? false,
    attachments: (msg.attachments || []).map((a: any) => ({
      filename: a.filename || '',
      size: a.size || undefined,
      contentType: a.contentType || undefined,
      downloadUrl: a.downloadUrl ? `${BASE_URL}${a.downloadUrl}` : undefined,
    })),
  };
}

/**
 * 获取邮件列表
 * 流程: GET /messages 获取列表 → 对每封邮件 GET /messages/{id} 获取详情（含 text/html）
 */
export async function getEmails(token: string, email: string): Promise<Email[]> {
  // 1. 获取邮件列表
  const listResponse = await fetch(`${BASE_URL}/messages`, {
    method: 'GET',
    headers: {
      ...DEFAULT_HEADERS,
      'Authorization': `Bearer ${token}`,
    },
  });

  if (!listResponse.ok) {
    throw new Error(`Failed to get emails: ${listResponse.status}`);
  }

  const listData = await listResponse.json();
  /* 兼容 Hydra 格式和纯数组格式 */
  const messages = Array.isArray(listData) ? listData : (listData['hydra:member'] || []);

  if (messages.length === 0) {
    return [];
  }

  // 2. 并行获取每封邮件的详情（含 text/html/attachments）
  const detailPromises = messages.map(async (msg: any) => {
    try {
      const detailResponse = await fetch(`${BASE_URL}/messages/${msg.id}`, {
        method: 'GET',
        headers: {
          ...DEFAULT_HEADERS,
          'Authorization': `Bearer ${token}`,
        },
      });

      if (!detailResponse.ok) {
        // 如果详情获取失败，退回使用列表数据
        return flattenMessage(msg, email);
      }

      const detail = await detailResponse.json();
      return flattenMessage(detail, email);
    } catch {
      // 出错时退回使用列表数据
      return flattenMessage(msg, email);
    }
  });

  const flatMessages = await Promise.all(detailPromises);
  return flatMessages.map((raw: any) => normalizeEmail(raw, email));
}
