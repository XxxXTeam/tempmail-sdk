import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'expressinboxhub';
const BASE = 'https://expressinboxhub.com';

/** CSRF token 提取正则 */
const CSRF_RE = /<meta\s+name="csrf-token"\s+content="([^"]+)"/i;

const DEFAULT_HEADERS: Record<string, string> = {
  Accept: 'application/json, text/plain, */*',
  'Accept-Language': 'zh-CN,zh;q=0.9,en;q=0.8',
  'Cache-Control': 'no-cache',
  DNT: '1',
  Pragma: 'no-cache',
  Referer: `${BASE}/`,
  'sec-ch-ua': '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
  'sec-ch-ua-mobile': '?0',
  'sec-ch-ua-platform': '"Windows"',
  'sec-fetch-dest': 'empty',
  'sec-fetch-mode': 'cors',
  'sec-fetch-site': 'same-origin',
  'User-Agent':
    'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
};

/** token 前缀，用于区分不同渠道的 token 格式 */
const TOK_PREFIX = 'eih1:';

/** 从 Set-Cookie 响应头中提取所有 cookie 行 */
function setCookieLines(headers: Headers): string[] {
  const h = headers as Headers & { getSetCookie?: () => string[] };
  if (typeof h.getSetCookie === 'function') {
    return h.getSetCookie();
  }
  const one = headers.get('set-cookie');
  return one ? [one] : [];
}

/** 解析 cookie 字符串为键值映射 */
function parseCookieMap(hdr: string): Map<string, string> {
  const m = new Map<string, string>();
  for (const part of hdr.split(';')) {
    const p = part.trim();
    if (!p || !p.includes('=')) continue;
    const i = p.indexOf('=');
    const k = p.slice(0, i).trim();
    const v = p.slice(i + 1).trim();
    if (k) m.set(k, v);
  }
  return m;
}

/** 合并已有 cookie 与响应头中的新 cookie */
function mergeCookieHeader(prev: string, headers: Headers): string {
  const m = parseCookieMap(prev);
  for (const line of setCookieLines(headers)) {
    const first = line.split(';')[0] ?? '';
    const i = first.indexOf('=');
    if (i <= 0) continue;
    const k = first.slice(0, i).trim();
    const v = first.slice(i + 1).trim();
    if (k) m.set(k, v);
  }
  return [...m.entries()]
    .sort((a, b) => a[0].localeCompare(b[0]))
    .map(([k, v]) => `${k}=${v}`)
    .join('; ');
}

/** 将 CSRF token 与 session cookie 编码为 SDK 内部 token */
function encToken(csrf: string, cookie: string): string {
  const raw = JSON.stringify({ csrf, cookie });
  return TOK_PREFIX + Buffer.from(raw, 'utf8').toString('base64');
}

/** 解码 SDK 内部 token，还原 CSRF token 与 session cookie */
function decToken(tok: string): { csrf: string; cookie: string } {
  if (!tok.startsWith(TOK_PREFIX)) {
    throw new Error('expressinboxhub: invalid token');
  }
  let raw: string;
  try {
    raw = Buffer.from(tok.slice(TOK_PREFIX.length), 'base64').toString('utf8');
  } catch {
    throw new Error('expressinboxhub: invalid token');
  }
  const o = JSON.parse(raw) as { csrf?: string; cookie?: string };
  const csrf = (o.csrf ?? '').trim();
  const cookie = (o.cookie ?? '').trim();
  if (!csrf || !cookie) throw new Error('expressinboxhub: invalid token');
  return { csrf, cookie };
}

/**
 * 创建临时邮箱
 *
 * 流程：
 * 1. GET 首页获取 CSRF token 和 session cookie
 * 2. POST /messages 使用 CSRF token 创建邮箱
 *
 * @returns 包含邮箱地址和内部 token 的邮箱信息
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  /* 第一步：获取首页 HTML，提取 CSRF token 和 session cookie */
  const r1 = await fetchWithTimeout(BASE, {
    method: 'GET',
    headers: {
      ...DEFAULT_HEADERS,
      Accept: 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
    },
  });
  if (!r1.ok) {
    throw new Error(`expressinboxhub: 获取首页失败 HTTP ${r1.status}`);
  }
  const html = await r1.text();
  let cookieHdr = mergeCookieHeader('', r1.headers);

  /* 从 HTML 中提取 CSRF token */
  const csrfMatch = CSRF_RE.exec(html);
  if (!csrfMatch?.[1]) {
    throw new Error('expressinboxhub: 未找到 CSRF token');
  }
  const csrfToken = csrfMatch[1];

  /* 第二步：POST /messages 创建邮箱 */
  const r2 = await fetchWithTimeout(`${BASE}/messages`, {
    method: 'POST',
    headers: {
      ...DEFAULT_HEADERS,
      'Content-Type': 'application/json',
      'X-CSRF-TOKEN': csrfToken,
      Cookie: cookieHdr,
    },
    body: JSON.stringify({ _token: csrfToken }),
  });
  if (!r2.ok) {
    throw new Error(`expressinboxhub: 创建邮箱失败 HTTP ${r2.status}`);
  }
  cookieHdr = mergeCookieHeader(cookieHdr, r2.headers);

  const data = (await r2.json()) as { mailbox?: string; messages?: unknown[] };
  if (!data.mailbox?.trim()) {
    throw new Error('expressinboxhub: 创建邮箱返回无效');
  }

  const email = data.mailbox.trim();
  const token = encToken(csrfToken, cookieHdr);

  return {
    channel: CHANNEL,
    email,
    token,
  };
}

/**
 * 获取邮件列表
 *
 * 使用创建邮箱时保存的 CSRF token 和 session cookie 进行请求
 *
 * @param token - SDK 内部 token（编码了 CSRF token 和 session cookie）
 * @param email - 邮箱地址
 * @returns 标准化后的邮件列表
 */
export async function getEmails(token: string, email: string): Promise<Email[]> {
  const { csrf, cookie } = decToken(token);

  const r = await fetchWithTimeout(`${BASE}/messages`, {
    method: 'POST',
    headers: {
      ...DEFAULT_HEADERS,
      'Content-Type': 'application/json',
      'X-CSRF-TOKEN': csrf,
      Cookie: cookie,
    },
    body: JSON.stringify({ _token: csrf }),
  });
  if (!r.ok) {
    throw new Error(`expressinboxhub: 获取邮件失败 HTTP ${r.status}`);
  }

  const data = (await r.json()) as { mailbox?: string; messages?: unknown[] };
  if (!Array.isArray(data.messages)) {
    return [];
  }

  return data.messages.map((raw) => normalizeEmail(raw, email));
}
