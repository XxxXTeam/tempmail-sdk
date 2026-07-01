/**
 * MinuteInbox 临时邮箱渠道
 * 网站: minuteinbox.com
 * Cookie Session + CSRF + REST JSON API
 */
import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'minuteinbox';
const BASE_URL = 'https://www.minuteinbox.com';

/** CSRF token 正则提取 */
const CSRF_RE = /CSRF\s*=\s*"([^"]+)"/;

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
    throw new Error('minuteinbox: token 解码失败');
  }
  const obj = JSON.parse(raw) as { csrf?: string; cookie?: string };
  if (!obj.csrf || !obj.cookie) {
    throw new Error('minuteinbox: token 缺少必要字段');
  }
  return { csrf: obj.csrf, cookie: obj.cookie };
}

/**
 * 创建 MinuteInbox 临时邮箱
 *
 * 流程:
 * 1. GET / → 获取 Session Cookie 和 CSRF token
 * 2. GET /index/index?csrf_token={csrf} → 获取邮箱地址
 * 3. token 中存储 CSRF 和 Cookie
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  /* 步骤 1: 访问首页获取 Session 和 CSRF */
  const r1 = await fetchWithTimeout(BASE_URL, {
    headers: COMMON_HEADERS,
    redirect: 'follow',
  });
  if (!r1.ok) {
    throw new Error(`minuteinbox: 获取首页失败 HTTP ${r1.status}`);
  }

  const html = await r1.text();
  let cookie = buildCookieHeader('', r1.headers);

  /* 提取 CSRF token */
  const csrfMatch = CSRF_RE.exec(html);
  if (!csrfMatch?.[1]) {
    throw new Error('minuteinbox: 无法从首页提取 CSRF token');
  }
  const csrf = csrfMatch[1];

  /* 步骤 2: 请求创建邮箱 */
  const createUrl = `${BASE_URL}/index/index?csrf_token=${encodeURIComponent(csrf)}`;
  const r2 = await fetchWithTimeout(createUrl, {
    headers: {
      ...COMMON_HEADERS,
      'Cookie': cookie,
      'X-Requested-With': 'XMLHttpRequest',
    },
  });
  if (!r2.ok) {
    throw new Error(`minuteinbox: 创建邮箱失败 HTTP ${r2.status}`);
  }

  cookie = buildCookieHeader(cookie, r2.headers);

  const data = await r2.json() as { email?: string };
  if (!data.email) {
    throw new Error('minuteinbox: 创建邮箱返回数据中缺少 email 字段');
  }

  const token = encodeToken(csrf, cookie);
  return {
    channel: CHANNEL,
    email: data.email,
    token,
  };
}

/**
 * 获取 MinuteInbox 邮箱的邮件列表
 *
 * 流程:
 * 1. GET /index/refresh → 邮件列表 JSON
 *    - 返回 0（数字）表示空收件箱
 *    - 返回数组，每个元素: {predmet, od, id, kdy, precteno}
 * 2. GET /email/id/{id} → 邮件 HTML 正文
 */
export async function getEmails(token: string, email: string): Promise<Email[]> {
  if (!token?.trim()) {
    throw new Error('minuteinbox: token 为空');
  }

  const { cookie } = decodeToken(token);

  /* 获取邮件列表 */
  const listUrl = `${BASE_URL}/index/refresh`;
  const r = await fetchWithTimeout(listUrl, {
    headers: {
      ...COMMON_HEADERS,
      'Cookie': cookie,
      'X-Requested-With': 'XMLHttpRequest',
    },
  });
  if (!r.ok) {
    throw new Error(`minuteinbox: 获取邮件列表失败 HTTP ${r.status}`);
  }

  const raw = await r.text();
  /* 空收件箱返回数字 0 */
  const trimmed = raw.trim();
  if (trimmed === '0' || trimmed === '') {
    return [];
  }

  let list: any[];
  try {
    const parsed = JSON.parse(trimmed);
    if (!Array.isArray(parsed)) {
      return [];
    }
    list = parsed;
  } catch {
    return [];
  }

  if (list.length === 0) {
    return [];
  }

  /* 获取每封邮件的详细内容 */
  const emails: Email[] = [];
  for (const item of list) {
    const id = item.id;
    if (!id) continue;

    const detailUrl = `${BASE_URL}/email/id/${id}`;
    const rd = await fetchWithTimeout(detailUrl, {
      headers: {
        ...COMMON_HEADERS,
        'Cookie': cookie,
        'X-Requested-With': 'XMLHttpRequest',
      },
    });
    if (!rd.ok) continue;

    const htmlBody = await rd.text();

    /*
     * 构建标准化数据对象
     * 捷克语字段映射: predmet=subject, od=from, kdy=when, precteno=read status
     */
    const mailData: Record<string, unknown> = {
      id: String(id),
      from: item.od || '',
      to: email,
      subject: item.predmet || '',
      html: htmlBody || '',
      date: item.kdy || '',
      isRead: item.precteno === 'precteno',
    };

    emails.push(normalizeEmail(mailData, email));
  }

  return emails;
}
