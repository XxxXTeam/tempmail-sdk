import { EmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';

const CHANNEL: Channel = 'awamail';
const BASE_URL = 'https://awamail.com/welcome';

const DEFAULT_HEADERS: Record<string, string> = {
  'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0',
  'Accept': 'application/json, text/javascript, */*; q=0.01',
  'accept-language': 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
  'cache-control': 'no-cache',
  'dnt': '1',
  'origin': 'https://awamail.com',
  'pragma': 'no-cache',
  'referer': 'https://awamail.com/?lang=zh',
  'sec-ch-ua': '"Not(A:Brand";v="8", "Chromium";v="144", "Microsoft Edge";v="144"',
  'sec-ch-ua-mobile': '?0',
  'sec-ch-ua-platform': '"Windows"',
  'sec-fetch-dest': 'empty',
  'sec-fetch-mode': 'cors',
  'sec-fetch-site': 'same-origin',
};

/**
 * 从 Set-Cookie 响应头中提取 awamail_session 值
 */
function extractSessionCookie(response: Response): string {
  const setCookie = response.headers.get('set-cookie') || '';
  const match = setCookie.match(/awamail_session=([^;]+)/);
  return match ? `awamail_session=${match[1]}` : '';
}

/**
 * 创建临时邮箱
 * API: POST /welcome/change_mailbox (空 body)
 * 返回: { success, data: { email_address, expired_at, created_at, ... } }
 * 需要保存响应中的 Set-Cookie (awamail_session) 用于后续获取邮件
 */
export async function generateEmail(): Promise<EmailInfo> {
  const response = await fetch(`${BASE_URL}/change_mailbox`, {
    method: 'POST',
    headers: {
      ...DEFAULT_HEADERS,
      'Content-Length': '0',
    },
  });

  if (!response.ok) {
    throw new Error(`Failed to generate email: ${response.status}`);
  }

  // 提取 session cookie
  const sessionCookie = extractSessionCookie(response);
  if (!sessionCookie) {
    throw new Error('Failed to extract session cookie');
  }

  const data = await response.json();

  if (!data.success || !data.data) {
    throw new Error('Failed to generate email');
  }

  return {
    channel: CHANNEL,
    email: data.data.email_address,
    token: sessionCookie,
    expiresAt: data.data.expired_at,
    createdAt: data.data.created_at,
  };
}

/**
 * 获取邮件列表
 * API: GET /welcome/get_emails
 * 需要传入 Cookie (awamail_session) 和 x-requested-with 头
 * 返回: { success, data: { emails: [...], latest: {...} } }
 */
export async function getEmails(token: string, email: string): Promise<Email[]> {
  const response = await fetch(`${BASE_URL}/get_emails`, {
    method: 'GET',
    headers: {
      ...DEFAULT_HEADERS,
      'Cookie': token,
      'x-requested-with': 'XMLHttpRequest',
    },
  });

  if (!response.ok) {
    throw new Error(`Failed to get emails: ${response.status}`);
  }

  const data = await response.json();

  if (!data.success || !data.data) {
    throw new Error('Failed to get emails');
  }

  const rawEmails = data.data.emails || [];
  return rawEmails.map((raw: any) => normalizeEmail(raw, email));
}
