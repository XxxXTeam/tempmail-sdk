/**
 * Emailnator 渠道实现
 * 网站: https://www.emailnator.com
 *
 * 特点:
 * - 需要 XSRF-TOKEN Session 认证
 * - 支持多种域名（包括 Gmail dot trick）
 * - 自动创建随机邮箱
 */

import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';

const CHANNEL: Channel = 'emailnator';
const BASE_URL = 'https://www.emailnator.com';

const DEFAULT_HEADERS: Record<string, string> = {
  'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/145.0.0.0 Safari/537.36',
  'Accept': 'application/json',
  'Content-Type': 'application/json',
  'X-Requested-With': 'XMLHttpRequest',
};

/**
 * 从 Set-Cookie 头中提取 XSRF-TOKEN
 */
function extractXsrfToken(setCookieHeaders: string[]): string {
  for (const cookie of setCookieHeaders) {
    const match = cookie.match(/XSRF-TOKEN=([^;]+)/);
    if (match) {
      return decodeURIComponent(match[1]);
    }
  }
  return '';
}

/**
 * 从响应头中提取所有 Set-Cookie 值
 */
function getSetCookieHeaders(response: Response): string[] {
  const cookies: string[] = [];
  response.headers.forEach((value, key) => {
    if (key.toLowerCase() === 'set-cookie') {
      cookies.push(value);
    }
  });
  return cookies;
}

/**
 * 初始化 Session，获取 XSRF-TOKEN 和 Session Cookie
 */
async function initSession(): Promise<{ xsrfToken: string; cookies: string }> {
  const response = await fetch(BASE_URL, {
    method: 'GET',
    headers: { 'User-Agent': DEFAULT_HEADERS['User-Agent'] },
    redirect: 'follow',
  });

  if (!response.ok) {
    throw new Error(`Failed to init session: ${response.status}`);
  }

  const setCookies = getSetCookieHeaders(response);
  const xsrfToken = extractXsrfToken(setCookies);

  /* 构建完整的 cookie 字符串 */
  const cookieParts: string[] = [];
  for (const cookie of setCookies) {
    const nameValue = cookie.split(';')[0];
    if (nameValue) cookieParts.push(nameValue);
  }

  if (!xsrfToken) {
    throw new Error('Failed to extract XSRF-TOKEN');
  }

  return { xsrfToken, cookies: cookieParts.join('; ') };
}

/**
 * 创建临时邮箱
 * 流程: 初始化 Session → POST /generate-email
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const { xsrfToken, cookies } = await initSession();

  const response = await fetch(`${BASE_URL}/generate-email`, {
    method: 'POST',
    headers: {
      ...DEFAULT_HEADERS,
      'X-XSRF-TOKEN': xsrfToken,
      'Cookie': cookies,
    },
    body: JSON.stringify({ email: ['domain'] }),
  });

  if (!response.ok) {
    throw new Error(`Failed to generate email: ${response.status}`);
  }

  const data = await response.json();
  const emailList = data.email;
  if (!emailList || emailList.length === 0) {
    throw new Error('Failed to generate email: empty response');
  }

  const email = emailList[0];

  /*
   * token 存储 XSRF-TOKEN + cookies 的组合
   * 后续获取邮件时需要这两个值
   */
  const tokenPayload = JSON.stringify({ xsrfToken, cookies });

  return {
    channel: CHANNEL,
    email: email,
    token: tokenPayload,
  };
}

/**
 * 获取邮件列表
 * API: POST /message-list
 */
export async function getEmails(token: string, email: string): Promise<Email[]> {
  const { xsrfToken, cookies } = JSON.parse(token);

  const response = await fetch(`${BASE_URL}/message-list`, {
    method: 'POST',
    headers: {
      ...DEFAULT_HEADERS,
      'X-XSRF-TOKEN': xsrfToken,
      'Cookie': cookies,
    },
    body: JSON.stringify({ email }),
  });

  if (!response.ok) {
    throw new Error(`Failed to get emails: ${response.status}`);
  }

  const data = await response.json();
  const messageData = data.messageData || [];

  /* 过滤广告消息（messageID 为 "ADSVPN" 等） */
  const realMessages = messageData.filter((m: any) =>
    m.messageID && !m.messageID.startsWith('ADS')
  );

  if (realMessages.length === 0) {
    return [];
  }

  /* 逐封获取邮件详情 */
  const detailPromises = realMessages.map(async (msg: any) => {
    try {
      const detailResp = await fetch(`${BASE_URL}/message-list`, {
        method: 'POST',
        headers: {
          ...DEFAULT_HEADERS,
          'X-XSRF-TOKEN': xsrfToken,
          'Cookie': cookies,
        },
        body: JSON.stringify({ email, messageID: msg.messageID }),
      });

      if (!detailResp.ok) return msg;
      const html = await detailResp.text();

      return {
        id: msg.messageID || '',
        from: msg.from || '',
        to: email,
        subject: msg.subject || '',
        text: '',
        html: html || '',
        date: msg.time || '',
        isRead: false,
        attachments: [],
      };
    } catch {
      return {
        id: msg.messageID || '',
        from: msg.from || '',
        to: email,
        subject: msg.subject || '',
        text: '',
        html: '',
        date: msg.time || '',
        isRead: false,
        attachments: [],
      };
    }
  });

  const details = await Promise.all(detailPromises);
  return details.map((raw: any) => normalizeEmail(raw, email));
}
