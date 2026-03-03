/**
 * mail.cx 渠道实现
 * API 文档: https://api.mail.cx/
 *
 * 特点:
 * - 无需注册，任意邮箱名即可接收
 * - JWT Token 认证
 * - 邮箱有效期 24 小时
 */

import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';

const CHANNEL: Channel = 'mail-cx';
const BASE_URL = 'https://api.mail.cx/api/v1';

/* mail.cx 可用域名列表（从页面抓取） */
const DOMAINS = ['qabq.com', 'nqmo.com'];

const DEFAULT_HEADERS: Record<string, string> = {
  'Content-Type': 'application/json',
  'Accept': 'application/json',
  'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/145.0.0.0 Safari/537.36',
};

/**
 * 生成随机用户名
 */
function randomUsername(length: number = 8): string {
  const chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
  let result = '';
  for (let i = 0; i < length; i++) {
    result += chars[Math.floor(Math.random() * chars.length)];
  }
  return result;
}

/**
 * 获取 JWT Token
 * API: POST /auth/authorize_token
 */
async function getToken(): Promise<string> {
  const response = await fetch(`${BASE_URL}/auth/authorize_token`, {
    method: 'POST',
    headers: DEFAULT_HEADERS,
    body: '{}',
  });

  if (!response.ok) {
    throw new Error(`Failed to get token: ${response.status}`);
  }

  const token = await response.text();
  /* 响应可能带引号，去除 */
  return token.replace(/^"|"$/g, '');
}

/**
 * 创建临时邮箱
 * mail.cx 无需注册，生成随机用户名 + 获取 JWT Token 即可
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const token = await getToken();
  const domain = DOMAINS[Math.floor(Math.random() * DOMAINS.length)];
  const username = randomUsername();
  const email = `${username}@${domain}`;

  return {
    channel: CHANNEL,
    email: email,
    token: token,
  };
}

/**
 * 获取邮件列表
 * API: GET /mailbox/{email}
 */
export async function getEmails(token: string, email: string): Promise<Email[]> {
  const response = await fetch(`${BASE_URL}/mailbox/${encodeURIComponent(email)}`, {
    method: 'GET',
    headers: {
      ...DEFAULT_HEADERS,
      'Authorization': `Bearer ${token}`,
    },
  });

  if (!response.ok) {
    throw new Error(`Failed to get emails: ${response.status}`);
  }

  const messages = await response.json();
  if (!Array.isArray(messages) || messages.length === 0) {
    return [];
  }

  /* 逐封获取详情 */
  const detailPromises = messages.map(async (msg: any) => {
    try {
      const detailResp = await fetch(`${BASE_URL}/mailbox/${encodeURIComponent(email)}/${msg.id}`, {
        method: 'GET',
        headers: {
          ...DEFAULT_HEADERS,
          'Authorization': `Bearer ${token}`,
        },
      });
      if (!detailResp.ok) return msg;
      return await detailResp.json();
    } catch {
      return msg;
    }
  });

  const details = await Promise.all(detailPromises);
  return details.map((raw: any) => normalizeEmail(raw, email));
}
