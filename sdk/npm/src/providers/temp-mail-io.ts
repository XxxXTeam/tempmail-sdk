import { EmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';

const CHANNEL: Channel = 'temp-mail-io';
const BASE_URL = 'https://api.internal.temp-mail.io/api/v3';

const DEFAULT_HEADERS: Record<string, string> = {
  'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0',
  'Content-Type': 'application/json',
  'accept-language': 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
  'application-name': 'web',
  'application-version': '4.0.0',
  'cache-control': 'no-cache',
  'dnt': '1',
  'origin': 'https://temp-mail.io',
  'pragma': 'no-cache',
  'referer': 'https://temp-mail.io/',
  'sec-ch-ua': '"Not(A:Brand";v="8", "Chromium";v="144", "Microsoft Edge";v="144"',
  'sec-ch-ua-mobile': '?0',
  'sec-ch-ua-platform': '"Windows"',
  'sec-fetch-dest': 'empty',
  'sec-fetch-mode': 'cors',
  'sec-fetch-site': 'same-site',
};

/**
 * 创建临时邮箱
 * API: POST /api/v3/email/new
 * 返回: { email, token }
 */
export async function generateEmail(): Promise<EmailInfo> {
  const response = await fetch(`${BASE_URL}/email/new`, {
    method: 'POST',
    headers: DEFAULT_HEADERS,
    body: JSON.stringify({ min_name_length: 100, max_name_length: 100 }),
  });

  if (!response.ok) {
    throw new Error(`Failed to generate email: ${response.status}`);
  }

  const data = await response.json();

  if (!data.email || !data.token) {
    throw new Error('Failed to generate email');
  }

  return {
    channel: CHANNEL,
    email: data.email,
    token: data.token,
  };
}

/**
 * 获取邮件列表
 * API: GET /api/v3/email/{email}/messages
 * 返回: [ { id, from, to, cc, subject, body_text, body_html, created_at, attachments } ]
 */
export async function getEmails(email: string): Promise<Email[]> {
  const encodedEmail = encodeURIComponent(email);
  const response = await fetch(`${BASE_URL}/email/${encodedEmail}/messages`, {
    method: 'GET',
    headers: DEFAULT_HEADERS,
  });

  if (!response.ok) {
    throw new Error(`Failed to get emails: ${response.status}`);
  }

  const data = await response.json();

  // API 直接返回邮件数组
  const rawEmails = Array.isArray(data) ? data : [];
  return rawEmails.map((raw: any) => normalizeEmail(raw, email));
}
