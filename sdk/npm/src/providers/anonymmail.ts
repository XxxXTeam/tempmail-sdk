import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'anonymmail';
const BASE = 'https://anonymmail.net';

const DEFAULT_HEADERS: Record<string, string> = {
  Accept: '*/*',
  'Content-Type': 'application/x-www-form-urlencoded',
  Referer: `${BASE}/`,
  'User-Agent':
    'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
};

interface DomainItem {
  domain?: string;
  expires?: string;
  left?: number;
}

/**
 * 生成随机用户名
 */
function randomUsername(length = 10): string {
  const chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
  let result = '';
  for (let i = 0; i < length; i++) {
    result += chars.charAt(Math.floor(Math.random() * chars.length));
  }
  return result;
}

/**
 * 获取可用域名列表
 * POST https://anonymmail.net/api/getDomains
 */
async function fetchDomains(): Promise<string[]> {
  const response = await fetchWithTimeout(`${BASE}/api/getDomains`, {
    method: 'POST',
    headers: DEFAULT_HEADERS,
  });
  if (!response.ok) {
    throw new Error(`anonymmail: 获取域名列表失败 HTTP ${response.status}`);
  }
  const data = (await response.json()) as DomainItem[];
  if (!Array.isArray(data) || data.length === 0) {
    throw new Error('anonymmail: 无可用域名');
  }
  const domains = data
    .map((d) => d.domain)
    .filter((d): d is string => typeof d === 'string' && d.trim().length > 0);
  if (domains.length === 0) {
    throw new Error('anonymmail: 无可用域名');
  }
  return domains;
}

/**
 * 创建 anonymmail.net 临时邮箱
 * 1. HEAD 获取 cookie session
 * 2. POST /api/create 创建邮箱
 */
export async function generateEmail(_domain?: string | null, channel: Channel = CHANNEL): Promise<InternalEmailInfo> {
  /* 获取可用域名 */
  const domains = await fetchDomains();
  const domain = domains[Math.floor(Math.random() * domains.length)];
  const username = randomUsername();
  const email = `${username}@${domain}`;

  /* HEAD 请求获取 session cookie */
  await fetchWithTimeout(BASE, {
    method: 'HEAD',
    headers: { 'User-Agent': DEFAULT_HEADERS['User-Agent'] },
  });

  /* POST /api/create 创建邮箱 */
  const response = await fetchWithTimeout(`${BASE}/api/create`, {
    method: 'POST',
    headers: DEFAULT_HEADERS,
    body: `email=${encodeURIComponent(email)}`,
  });
  if (!response.ok) {
    throw new Error(`anonymmail: 创建邮箱失败 HTTP ${response.status}`);
  }
  const data = (await response.json()) as { success?: boolean; email?: string };
  if (!data.success) {
    throw new Error('anonymmail: 创建邮箱返回失败');
  }

  return {
    channel,
    email: data.email?.trim() || email,
  };
}

/**
 * 获取 anonymmail.net 邮件列表
 * POST https://anonymmail.net/api/get body: email={email}
 * 响应格式: { "email@domain": { "created_at": "...", "emails": [...] } }
 */
export async function getEmails(email: string): Promise<Email[]> {
  if (!email?.trim()) {
    throw new Error('anonymmail: 邮箱地址为空');
  }
  const addr = email.trim();
  const response = await fetchWithTimeout(`${BASE}/api/get`, {
    method: 'POST',
    headers: DEFAULT_HEADERS,
    body: `email=${encodeURIComponent(addr)}`,
  });
  if (!response.ok) {
    throw new Error(`anonymmail: 获取邮件失败 HTTP ${response.status}`);
  }
  const data = await response.json();
  if (!data || typeof data !== 'object') {
    return [];
  }
  /* 响应是以邮箱地址为 key 的对象 */
  const inbox = data[addr];
  if (!inbox || !Array.isArray(inbox.emails)) {
    return [];
  }
  return inbox.emails.map((raw: unknown) => normalizeEmail(raw, addr));
}
