/**
 * tempemail.info 临时邮箱渠道
 *
 * 流程：
 *   1. GET / 获取 PHPSESSID 会话 Cookie，并从 HTML 解析 base64 编码的邮箱地址
 *   2. POST /template/checker.php（body: last_id=0）获取邮件列表（JSON 数组）
 *   3. GET /view/{date} 获取单封邮件 HTML 正文
 * token 存储会话 Cookie 串，用于后续请求绑定收件箱。
 */
import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'tempemail-info';
const BASE_URL = 'https://tempemail.info';

/** 匹配首页 HTML 中的 var emailEncoded = "base64..." */
const EMAIL_RE = /var\s+emailEncoded\s*=\s*"([^"]+)"/;

const USER_AGENT =
  'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0';

interface CheckerRow {
  id?: string | number;
  name?: string;
  from?: string;
  subject?: string;
  date?: string;
  read?: boolean;
}

function commonHeaders(): Record<string, string> {
  return {
    'User-Agent': USER_AGENT,
    'Accept-Language': 'zh-CN,zh;q=0.9,en;q=0.8',
    Referer: `${BASE_URL}/`,
    Origin: BASE_URL,
  };
}

/** 从 Set-Cookie 响应头提取 cookie 行（Node 下 Headers 可能有 getSetCookie） */
function setCookieLines(headers: Headers): string[] {
  const h = headers as Headers & { getSetCookie?: () => string[] };
  if (typeof h.getSetCookie === 'function') {
    return h.getSetCookie();
  }
  const one = headers.get('set-cookie');
  return one ? [one] : [];
}

/** 将 Set-Cookie 合并进既有 Cookie 请求头值 */
function mergeCookieHeader(prev: string, headers: Headers): string {
  const map = new Map<string, string>();
  if (prev) {
    for (const part of prev.split(';')) {
      const eq = part.indexOf('=');
      if (eq <= 0) continue;
      map.set(part.slice(0, eq).trim(), part.slice(eq + 1).trim());
    }
  }
  for (const line of setCookieLines(headers)) {
    const first = line.split(';')[0] ?? '';
    const eq = first.indexOf('=');
    if (eq <= 0) continue;
    map.set(first.slice(0, eq).trim(), first.slice(eq + 1).trim());
  }
  return [...map.entries()].map(([k, v]) => `${k}=${v}`).join('; ');
}

/**
 * 创建 tempemail.info 临时邮箱
 * GET / 获取会话 Cookie，并从 HTML 提取 emailEncoded（base64）解码为邮箱地址。
 * token 存储会话 Cookie 串，供后续 checker.php / view 请求使用。
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const r = await fetchWithTimeout(`${BASE_URL}/`, {
    headers: {
      ...commonHeaders(),
      Accept: 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
    },
  });
  if (!r.ok) {
    throw new Error(`tempemail-info: 获取首页失败 HTTP ${r.status}`);
  }

  const cookieHdr = mergeCookieHeader('', r.headers);
  if (!cookieHdr) {
    throw new Error('tempemail-info: 未获取到会话 Cookie');
  }

  const html = await r.text();
  const m = EMAIL_RE.exec(html);
  if (!m?.[1]) {
    throw new Error('tempemail-info: 未在页面中找到 emailEncoded');
  }

  let email: string;
  try {
    email = Buffer.from(m[1], 'base64').toString('utf8').trim();
  } catch {
    throw new Error('tempemail-info: 邮箱地址 base64 解码失败');
  }
  if (!email || !email.includes('@')) {
    throw new Error('tempemail-info: 解码出的邮箱地址无效');
  }

  return { channel: CHANNEL, email, token: cookieHdr };
}

/**
 * 获取 tempemail.info 邮件列表
 * 1. POST /template/checker.php（body: last_id=0）获取邮件列表 JSON 数组
 * 2. 对每封邮件 GET /view/{date} 获取 HTML 正文
 */
export async function getEmails(token: string, email: string): Promise<Email[]> {
  const cookieHdr = (token ?? '').trim();
  if (!cookieHdr) {
    throw new Error('tempemail-info: 缺少会话 Cookie');
  }

  const r = await fetchWithTimeout(`${BASE_URL}/template/checker.php`, {
    method: 'POST',
    headers: {
      ...commonHeaders(),
      Accept: 'application/json, text/javascript, */*; q=0.01',
      'X-Requested-With': 'XMLHttpRequest',
      'Content-Type': 'application/x-www-form-urlencoded',
      Cookie: cookieHdr,
    },
    body: 'last_id=0',
  });
  if (!r.ok) {
    throw new Error(`tempemail-info: 获取邮件列表失败 HTTP ${r.status}`);
  }

  let rows: CheckerRow[];
  try {
    const parsed = JSON.parse(await r.text());
    rows = Array.isArray(parsed) ? parsed : [];
  } catch {
    return [];
  }
  if (rows.length === 0) {
    return [];
  }

  const emails: Email[] = [];
  for (const row of rows) {
    /* 构造发件人地址：有显示名且不同于地址则格式化为 "name <email>" */
    let fromAddr = row.from ?? '';
    if (row.name && row.name !== row.from) {
      fromAddr = `${row.name} <${row.from ?? ''}>`;
    }

    const htmlBody = await fetchBody(cookieHdr, row.date ?? '');

    const raw: Record<string, unknown> = {
      id: String(row.id ?? ''),
      from: fromAddr,
      to: email,
      subject: row.subject ?? '',
      date: row.date ?? '',
      html: htmlBody,
      isRead: Boolean(row.read),
    };
    emails.push(normalizeEmail(raw, email));
  }

  return emails;
}

/**
 * 获取单封邮件的 HTML 正文
 * date 作为路径段需进行 URL 编码。
 */
async function fetchBody(cookieHdr: string, date: string): Promise<string> {
  if (!date.trim()) {
    return '';
  }
  const r = await fetchWithTimeout(`${BASE_URL}/view/${encodeURIComponent(date)}`, {
    headers: {
      ...commonHeaders(),
      Accept: 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
      Cookie: cookieHdr,
    },
  });
  if (!r.ok) {
    return '';
  }
  return await r.text();
}
