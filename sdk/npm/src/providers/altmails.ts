/**
 * AltMails 临时邮箱渠道
 * 网站: tempmail.altmails.com
 * Cookie Session + CSRF + REST JSON API
 *
 * 流程:
 * - Generate: GET / 获取 Session Cookie 和 CSRF → GET /random-email-address 获取随机邮箱
 * - GetEmails: POST /fetch-emails/{email} 获取邮件列表 → GET /view/{id} 获取邮件正文
 */
import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'altmails';
const BASE_URL = 'https://tempmail.altmails.com';

/** 从 HTML inline script 中提取 CSRF token: '_token': 'xxx' */
const CSRF_RE = /'_token'\s*:\s*'([^']+)'/;

/** 公共请求头 */
const COMMON_HEADERS: Record<string, string> = {
  'Accept': 'application/json, text/html, */*',
  'Accept-Language': 'zh-CN,zh;q=0.9,en;q=0.8',
  'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36',
  'Referer': `${BASE_URL}/`,
};

/**
 * 从 Set-Cookie 响应头中提取 cookie 字符串
 * Node.js 环境下 Headers 可能有 getSetCookie 方法
 */
function extractSetCookies(headers: Headers): string[] {
  const h = headers as Headers & { getSetCookie?: () => string[] };
  if (typeof h.getSetCookie === 'function') {
    return h.getSetCookie();
  }
  const one = headers.get('set-cookie');
  return one ? [one] : [];
}

/**
 * 将多个 Set-Cookie 头合并为一个 Cookie 请求头值
 */
function buildCookieHeader(prevCookie: string, headers: Headers): string {
  const map = new Map<string, string>();
  /* 解析已有 cookie */
  if (prevCookie) {
    for (const part of prevCookie.split(';')) {
      const eq = part.indexOf('=');
      if (eq <= 0) continue;
      map.set(part.slice(0, eq).trim(), part.slice(eq + 1).trim());
    }
  }
  /* 合并新的 Set-Cookie */
  for (const line of extractSetCookies(headers)) {
    const first = line.split(';')[0] ?? '';
    const eq = first.indexOf('=');
    if (eq <= 0) continue;
    map.set(first.slice(0, eq).trim(), first.slice(eq + 1).trim());
  }
  return [...map.entries()].map(([k, v]) => `${k}=${v}`).join('; ');
}

/**
 * token 编解码
 * token 内部存储 JSON: {csrf, cookie}
 */
function encodeToken(csrf: string, cookie: string): string {
  return Buffer.from(JSON.stringify({ csrf, cookie }), 'utf8').toString('base64');
}

function decodeToken(token: string): { csrf: string; cookie: string } {
  let raw: string;
  try {
    raw = Buffer.from(token, 'base64').toString('utf8');
  } catch {
    throw new Error('altmails: token 解码失败');
  }
  const obj = JSON.parse(raw) as { csrf?: string; cookie?: string };
  if (!obj.csrf || !obj.cookie) {
    throw new Error('altmails: token 缺少必要字段');
  }
  return { csrf: obj.csrf, cookie: obj.cookie };
}

/**
 * 创建 AltMails 临时邮箱
 *
 * 流程:
 * 1. GET / → 获取 Session Cookie 和 CSRF token（从 inline script 中提取 '_token': 'xxx'）
 * 2. GET /random-email-address → 纯文本邮箱地址
 * 3. token 中存储 CSRF 和 Cookie
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  /* 步骤 1: 访问首页获取 Session Cookie 和 CSRF */
  const r1 = await fetchWithTimeout(BASE_URL, {
    headers: COMMON_HEADERS,
    redirect: 'follow',
  });
  if (!r1.ok) {
    throw new Error(`altmails: 获取首页失败 HTTP ${r1.status}`);
  }

  const html = await r1.text();
  let cookie = buildCookieHeader('', r1.headers);

  /* 提取 CSRF token */
  const csrfMatch = CSRF_RE.exec(html);
  if (!csrfMatch?.[1]) {
    throw new Error('altmails: 无法从首页提取 CSRF token');
  }
  const csrf = csrfMatch[1];

  /* 步骤 2: GET /random-email-address 获取随机邮箱地址 */
  const r2 = await fetchWithTimeout(`${BASE_URL}/random-email-address`, {
    headers: {
      ...COMMON_HEADERS,
      'Cookie': cookie,
    },
    redirect: 'follow',
  });
  if (!r2.ok) {
    throw new Error(`altmails: 获取随机邮箱失败 HTTP ${r2.status}`);
  }

  cookie = buildCookieHeader(cookie, r2.headers);

  const emailAddress = (await r2.text()).trim();
  if (!emailAddress || !emailAddress.includes('@')) {
    throw new Error('altmails: 返回的邮箱地址格式无效');
  }

  const token = encodeToken(csrf, cookie);
  return {
    channel: CHANNEL,
    email: emailAddress,
    token,
  };
}

/**
 * 获取 AltMails 邮箱的邮件列表
 *
 * 流程:
 * 1. POST /fetch-emails/{email} → 邮件列表 JSON 数组
 *    - 空数组 [] 表示无邮件
 *    - 包含邮件对象: {id, from, subject, ...}
 * 2. GET /view/{id} → 邮件 HTML 正文页面
 */
export async function getEmails(token: string, email: string): Promise<Email[]> {
  if (!token?.trim()) {
    throw new Error('altmails: token 为空');
  }

  const { csrf, cookie } = decodeToken(token);

  /* 步骤 1: 获取邮件列表 */
  const r = await fetchWithTimeout(`${BASE_URL}/fetch-emails/${encodeURIComponent(email)}`, {
    method: 'POST',
    headers: {
      ...COMMON_HEADERS,
      'Cookie': cookie,
      'X-Requested-With': 'XMLHttpRequest',
      'Accept': 'application/json',
      'Content-Type': 'application/x-www-form-urlencoded',
    },
    body: `_token=${encodeURIComponent(csrf)}`,
  });
  if (!r.ok) {
    throw new Error(`altmails: 获取邮件列表失败 HTTP ${r.status}`);
  }

  const data = await r.json() as any[];
  if (!Array.isArray(data) || data.length === 0) {
    return [];
  }

  /* 步骤 2: 逐条获取邮件 HTML 正文 */
  const emails: Email[] = [];
  for (const msg of data) {
    const id = msg.id;
    if (!id) continue;

    /* 获取邮件正文 HTML */
    let htmlBody = '';
    try {
      const rd = await fetchWithTimeout(`${BASE_URL}/view/${id}`, {
        headers: {
          ...COMMON_HEADERS,
          'Cookie': cookie,
        },
      });
      if (rd.ok) {
        htmlBody = await rd.text();
      }
    } catch {
      /* 正文获取失败不影响整体流程，仅正文为空 */
    }

    const mailData: Record<string, unknown> = {
      id: String(id),
      from: msg.from || '',
      to: email,
      subject: msg.subject || '',
      html: htmlBody,
      date: msg.created_at || msg.date || '',
    };

    emails.push(normalizeEmail(mailData, email));
  }

  return emails;
}
