import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'mail-tm';
const BASE_URL = 'https://api.mail.tm';

/**
 * 与 Internxt 临时邮箱页（https://internxt.com/temporary-email）前端一致：
 * 同源 cross-site 请求 api.mail.tm 时常带 Origin/Referer；domains 可为空 Bearer。
 */
const DEFAULT_HEADERS: Record<string, string> = {
  'Accept': 'application/json',
  'Accept-Language': 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
  'Cache-Control': 'no-cache',
  'Pragma': 'no-cache',
  'Origin': 'https://internxt.com',
  'Referer': 'https://internxt.com/',
  'User-Agent':
    'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
};

function jsonHeaders(extra?: Record<string, string>): Record<string, string> {
  return { ...DEFAULT_HEADERS, 'Content-Type': 'application/json', ...extra };
}

function bearerHeaders(token: string, extra?: Record<string, string>): Record<string, string> {
  return { ...jsonHeaders(extra), Authorization: `Bearer ${token}` };
}

function parseHydraMember(data: unknown): any[] {
  if (Array.isArray(data)) return data;
  if (data && typeof data === 'object') {
    const o = data as Record<string, unknown>;
    if (Array.isArray(o['hydra:member'])) return o['hydra:member'];
    if (Array.isArray(o['data'])) return o['data'];
  }
  return [];
}

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
  const response = await fetchWithTimeout(`${BASE_URL}/domains?page=1`, {
    method: 'GET',
    headers: DEFAULT_HEADERS,
  });

  if (!response.ok) {
    throw new Error(`Failed to get domains: ${response.status}`);
  }

  const data = await response.json();
  const members = parseHydraMember(data);
  return members
    .filter((d: any) => d && d.isActive !== false)
    .map((d: any) => d.domain)
    .filter(Boolean);
}

/**
 * 创建账号
 * API: POST /accounts
 */
async function createAccount(address: string, password: string): Promise<any> {
  const response = await fetchWithTimeout(`${BASE_URL}/accounts`, {
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
  const response = await fetchWithTimeout(`${BASE_URL}/token`, {
    method: 'POST',
    headers: jsonHeaders(),
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
export async function generateEmail(): Promise<InternalEmailInfo> {
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
  const listResponse = await fetchWithTimeout(`${BASE_URL}/messages?page=1`, {
    method: 'GET',
    headers: bearerHeaders(token),
  });

  if (!listResponse.ok) {
    throw new Error(`Failed to get emails: ${listResponse.status}`);
  }

  const listData = await listResponse.json();
  const messages = parseHydraMember(listData);

  if (messages.length === 0) {
    return [];
  }

  // 2. 并行获取每封邮件的详情（含 text/html/attachments）
  const detailPromises = messages.map(async (msg: any) => {
    try {
      const detailResponse = await fetchWithTimeout(`${BASE_URL}/messages/${msg.id}`, {
        method: 'GET',
        headers: bearerHeaders(token),
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
