import { EmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';

const CHANNEL: Channel = 'tempmail-la';
const BASE_URL = 'https://tempmail.la/api';

const DEFAULT_HEADERS: Record<string, string> = {
  'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0',
  'Accept': 'application/json, text/plain, */*',
  'Content-Type': 'application/json',
  'accept-language': 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
  'cache-control': 'no-cache',
  'dnt': '1',
  'locale': 'zh-CN',
  'origin': 'https://tempmail.la',
  'platform': 'PC',
  'pragma': 'no-cache',
  'product': 'TEMP_MAIL',
  'referer': 'https://tempmail.la/zh-CN/tempmail',
  'sec-ch-ua': '"Not(A:Brand";v="8", "Chromium";v="144", "Microsoft Edge";v="144"',
  'sec-ch-ua-mobile': '?0',
  'sec-ch-ua-platform': '"Windows"',
  'sec-fetch-dest': 'empty',
  'sec-fetch-mode': 'cors',
  'sec-fetch-site': 'same-origin',
};

/**
 * 创建临时邮箱
 * API: POST /api/mail/create
 * 返回: { code: 0, data: { mailId, address, type, startAt, endAt } }
 */
export async function generateEmail(): Promise<EmailInfo> {
  const response = await fetch(`${BASE_URL}/mail/create`, {
    method: 'POST',
    headers: DEFAULT_HEADERS,
    body: JSON.stringify({ turnstile: '' }),
  });

  if (!response.ok) {
    throw new Error(`Failed to generate email: ${response.status}`);
  }

  const data = await response.json();

  if (data.code !== 0 || !data.data) {
    throw new Error('Failed to generate email');
  }

  return {
    channel: CHANNEL,
    email: data.data.address,
    expiresAt: data.data.endAt,
    createdAt: data.data.startAt,
  };
}

/**
 * 获取邮件列表
 * API: POST /api/mail/box
 * 请求: { address, cursor }
 * 返回: { code: 0, data: { rows: [...], cursor, hasMore } }
 * 每封邮件: { mailboxId, messageFrom, subject, createdAt, html, attachments, read }
 */
export async function getEmails(email: string): Promise<Email[]> {
  const allEmails: any[] = [];
  let cursor: string | null = null;
  let hasMore = true;

  // 支持分页，循环获取所有邮件
  while (hasMore) {
    const response: Response = await fetch(`${BASE_URL}/mail/box`, {
      method: 'POST',
      headers: DEFAULT_HEADERS,
      body: JSON.stringify({ address: email, cursor }),
    });

    if (!response.ok) {
      throw new Error(`Failed to get emails: ${response.status}`);
    }

    const data: any = await response.json();

    if (data.code !== 0 || !data.data) {
      throw new Error('Failed to get emails');
    }

    const rows = data.data.rows || [];
    allEmails.push(...rows);

    if (data.data.hasMore && data.data.cursor) {
      cursor = data.data.cursor;
    } else {
      hasMore = false;
    }
  }

  return allEmails.map((raw: any) => normalizeEmail(raw, email));
}
