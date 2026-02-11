import { EmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';

const CHANNEL: Channel = 'temp-mail-io';
const BASE_URL = 'https://api.internal.temp-mail.io/api/v3';
const PAGE_URL = 'https://temp-mail.io/en';

/**
 * 缓存从页面动态获取的 mobileTestingHeader 值（用于 X-CORS-Header）
 */
let cachedCorsHeader: string | null = null;

/**
 * 从 temp-mail.io 页面的 __NUXT__ 运行时配置中提取 mobileTestingHeader
 * 该值用于 API 请求的 X-CORS-Header 头，缺少此头会导致 400 错误
 */
async function fetchCorsHeader(): Promise<string> {
  if (cachedCorsHeader) return cachedCorsHeader;

  try {
    const response = await fetch(PAGE_URL, {
      headers: {
        'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36',
      },
    });
    const html = await response.text();
    const match = html.match(/mobileTestingHeader\s*:\s*"([^"]+)"/);
    if (match) {
      cachedCorsHeader = match[1];
      return cachedCorsHeader;
    }
  } catch {
    /* 提取失败时使用默认值 */
  }

  cachedCorsHeader = '1';
  return cachedCorsHeader;
}

/**
 * 构建 API 请求头
 * 关键头: Content-Type, Application-Name, Application-Version, X-CORS-Header
 */
async function getApiHeaders(): Promise<Record<string, string>> {
  const corsHeader = await fetchCorsHeader();
  return {
    'Content-Type': 'application/json',
    'Application-Name': 'web',
    'Application-Version': '4.0.0',
    'X-CORS-Header': corsHeader,
    'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0',
    'origin': 'https://temp-mail.io',
    'referer': 'https://temp-mail.io/',
  };
}

/**
 * 创建临时邮箱
 * API: POST /api/v3/email/new
 * 返回: { email, token }
 */
export async function generateEmail(): Promise<EmailInfo> {
  const headers = await getApiHeaders();
  const response = await fetch(`${BASE_URL}/email/new`, {
    method: 'POST',
    headers,
    body: JSON.stringify({ min_name_length: 10, max_name_length: 10 }),
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
  const headers = await getApiHeaders();
  const response = await fetch(`${BASE_URL}/email/${email}/messages`, {
    method: 'GET',
    headers,
  });

  if (!response.ok) {
    throw new Error(`Failed to get emails: ${response.status}`);
  }

  const data = await response.json();

  // API 直接返回邮件数组
  const rawEmails = Array.isArray(data) ? data : [];
  return rawEmails.map((raw: any) => normalizeEmail(raw, email));
}
