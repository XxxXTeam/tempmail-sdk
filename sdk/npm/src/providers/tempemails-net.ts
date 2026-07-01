/**
 * TempEmails.net 临时邮箱渠道
 * 网站: tempemails.net
 * Laravel Cookie Session + CSRF + REST JSON API
 *
 * 流程:
 * - Generate: GET / 获取 Session Cookie 和 CSRF → POST /get_messages 获取自动分配的邮箱
 * - GetEmails: POST /get_messages 获取邮件列表 → GET /view/{id} 获取邮件正文
 */
import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'tempemails-net';
const BASE_URL = 'https://tempemails.net';

/** 从 HTML 中提取 CSRF token: <meta name="csrf-token" content="xxx"> */
const CSRF_RE = /<meta\s+name="csrf-token"\s+content="([^"]+)"/i;

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
    throw new Error('tempemails-net: token 解码失败');
  }
  const obj = JSON.parse(raw) as { csrf?: string; cookie?: string };
  if (!obj.csrf || !obj.cookie) {
    throw new Error('tempemails-net: token 缺少必要字段');
  }
  return { csrf: obj.csrf, cookie: obj.cookie };
}

/**
 * 创建 TempEmails.net 临时邮箱
 *
 * 流程:
 * 1. GET / → 获取 Session Cookie（XSRF-TOKEN、temp_mail_session）和 CSRF token
 * 2. POST /get_messages → 获取服务端自动分配的邮箱地址
 * 3. token 中存储 CSRF 和 Cookie
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  /* 步骤 1: 访问首页获取 Session 和 CSRF */
  const r1 = await fetchWithTimeout(BASE_URL, {
    headers: COMMON_HEADERS,
    redirect: 'follow',
  });
  if (!r1.ok) {
    throw new Error(`tempemails-net: 获取首页失败 HTTP ${r1.status}`);
  }

  const html = await r1.text();
  let cookie = buildCookieHeader('', r1.headers);

  /* 提取 CSRF token */
  const csrfMatch = CSRF_RE.exec(html);
  if (!csrfMatch?.[1]) {
    throw new Error('tempemails-net: 无法从首页提取 CSRF token');
  }
  const csrf = csrfMatch[1];

  /* 步骤 2: POST /get_messages 获取自动分配的邮箱地址 */
  const r2 = await fetchWithTimeout(`${BASE_URL}/get_messages`, {
    method: 'POST',
    headers: {
      ...COMMON_HEADERS,
      'Cookie': cookie,
      'X-Requested-With': 'XMLHttpRequest',
      'X-CSRF-TOKEN': csrf,
      'Accept': 'application/json',
    },
  });
  if (!r2.ok) {
    throw new Error(`tempemails-net: 获取邮箱失败 HTTP ${r2.status}`);
  }

  cookie = buildCookieHeader(cookie, r2.headers);

  const data = await r2.json() as {
    status?: boolean;
    mailbox?: string;
    email_token?: string;
  };
  if (!data.status || !data.mailbox) {
    throw new Error('tempemails-net: 获取邮箱返回数据缺少 mailbox 字段');
  }

  const token = encodeToken(csrf, cookie);
  return {
    channel: CHANNEL,
    email: data.mailbox,
    token,
  };
}

/**
 * 获取 TempEmails.net 邮箱的邮件列表
 *
 * 流程:
 * 1. POST /get_messages → 邮件列表 JSON
 *    - messages 为空数组表示无邮件
 *    - messages 包含邮件对象: {id, from, from_email, subject, receivedAt, ...}
 * 2. GET /view/{id} → 邮件 HTML 正文
 */
export async function getEmails(token: string, email: string): Promise<Email[]> {
  if (!token?.trim()) {
    throw new Error('tempemails-net: token 为空');
  }

  const { csrf, cookie } = decodeToken(token);

  /* 步骤 1: 获取邮件列表 */
  const r = await fetchWithTimeout(`${BASE_URL}/get_messages`, {
    method: 'POST',
    headers: {
      ...COMMON_HEADERS,
      'Cookie': cookie,
      'X-Requested-With': 'XMLHttpRequest',
      'X-CSRF-TOKEN': csrf,
      'Accept': 'application/json',
    },
  });
  if (!r.ok) {
    throw new Error(`tempemails-net: 获取邮件列表失败 HTTP ${r.status}`);
  }

  const data = await r.json() as {
    status?: boolean;
    mailbox?: string;
    messages?: any[];
  };

  if (!data.status || !data.messages || data.messages.length === 0) {
    return [];
  }

  /* 步骤 2: 逐条获取邮件 HTML 正文 */
  const emails: Email[] = [];
  for (const msg of data.messages) {
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
      from: msg.from_email || msg.from || '',
      to: email,
      subject: msg.subject || '',
      html: htmlBody,
      date: msg.receivedAt || msg.created_at || '',
    };

    emails.push(normalizeEmail(mailData, email));
  }

  return emails;
}
