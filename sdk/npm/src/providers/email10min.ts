import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'email10min';
const BASE_URL = 'https://email10min.com';

const BROWSER_HEADERS: Record<string, string> = {
  'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8',
  'Accept-Language': 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
  'Cache-Control': 'no-cache',
  'DNT': '1',
  'Pragma': 'no-cache',
  'Upgrade-Insecure-Requests': '1',
  'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
  'sec-ch-ua': '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
  'sec-ch-ua-mobile': '?0',
  'sec-ch-ua-platform': '"Windows"',
};

const AJAX_HEADERS: Record<string, string> = {
  'Accept': 'application/json, text/plain, */*',
  'Accept-Language': 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
  'Content-Type': 'application/x-www-form-urlencoded; charset=UTF-8',
  'Origin': BASE_URL,
  'Referer': `${BASE_URL}/zh`,
  'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
  'X-Requested-With': 'XMLHttpRequest',
  'sec-ch-ua': '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
  'sec-ch-ua-mobile': '?0',
  'sec-ch-ua-platform': '"Windows"',
  'sec-fetch-dest': 'empty',
  'sec-fetch-mode': 'cors',
  'sec-fetch-site': 'same-origin',
};

/** 从响应头提取 `name=value` Cookie 串 */
function cookieHeaderFromResponse(headers: Headers): string {
  const h = headers as Headers & { getSetCookie?: () => string[] };
  const lines = typeof h.getSetCookie === 'function' ? h.getSetCookie() : [];
  const parts: string[] = [];
  for (const line of lines) {
    const nv = line.split(';')[0].trim();
    if (nv) parts.push(nv);
  }
  if (!parts.length) {
    const single = headers.get('set-cookie');
    if (single) {
      for (const segment of single.split(/,(?=[^ =]+=)/)) {
        const nv = segment.split(';')[0].trim();
        if (nv) parts.push(nv);
      }
    }
  }
  return parts.join('; ');
}

/** 合并已有 Cookie 与新响应中的 Cookie */
function mergeCookieHeader(existing: string, headers: Headers): string {
  const incoming = cookieHeaderFromResponse(headers);
  if (!incoming) return existing;
  const map = new Map<string, string>();
  for (const chunk of [existing, incoming]) {
    for (const pair of chunk.split(';')) {
      const trimmed = pair.trim();
      if (!trimmed) continue;
      const eq = trimmed.indexOf('=');
      if (eq <= 0) continue;
      const name = trimmed.slice(0, eq).trim();
      const value = trimmed.slice(eq + 1).trim();
      map.set(name, value);
    }
  }
  return [...map.entries()].map(([k, v]) => `${k}=${v}`).join('; ');
}

/** 从 HTML 页面提取 CSRF token */
function extractCsrfToken(html: string): string {
  const metaMatch = html.match(/<meta\s+name="csrf-token"\s+content="([^"]+)"/i);
  if (metaMatch?.[1]) return metaMatch[1];
  /* 备用：尝试 input hidden */
  const inputMatch = html.match(/<input[^>]+name="_token"[^>]+value="([^"]+)"/i);
  if (inputMatch?.[1]) return inputMatch[1];
  throw new Error('email10min: 未找到 CSRF token');
}

/** 从 HTML 页面提取邮箱地址 */
function extractEmailFromHtml(html: string): string {
  /* 尝试 id="emailAddress" 或 class="emailAddress" 的元素 */
  const idMatch = html.match(/id="emailAddress"[^>]*>([^<]+)</i);
  if (idMatch?.[1]?.includes('@')) return idMatch[1].trim();
  /* 尝试 class 匹配 */
  const clsMatch = html.match(/class="[^"]*email[^"]*"[^>]*>([^<]*@[^<]+)</i);
  if (clsMatch?.[1]) return clsMatch[1].trim();
  /* 尝试 data-email 属性 */
  const dataMatch = html.match(/data-email="([^"]+@[^"]+)"/i);
  if (dataMatch?.[1]) return dataMatch[1].trim();
  /* 尝试 value 属性包含邮箱的 input */
  const valueMatch = html.match(/value="([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})"/);
  if (valueMatch?.[1]) return valueMatch[1].trim();
  /* 尝试 mailbox JSON 数据 */
  const jsonMatch = html.match(/"mailbox"\s*:\s*"([^"]+@[^"]+)"/);
  if (jsonMatch?.[1]) return jsonMatch[1].trim();
  /* 最后尝试：页面中任何合法的临时邮箱地址 */
  const genericMatch = html.match(/([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})/);
  if (genericMatch?.[1]) {
    const addr = genericMatch[1];
    /* 排除常见的非临时邮箱地址 */
    if (!addr.includes('email10min') && !addr.includes('example') && !addr.includes('google')) {
      return addr.trim();
    }
  }
  throw new Error('email10min: 未找到邮箱地址');
}

/** token 编解码：存储 cookie + csrf */
const TOK_PREFIX = 'e10m:';

function encodeToken(cookie: string, csrf: string): string {
  const raw = JSON.stringify({ c: cookie, t: csrf });
  return TOK_PREFIX + Buffer.from(raw, 'utf8').toString('base64');
}

function decodeToken(token: string): { cookie: string; csrf: string } {
  if (!token.startsWith(TOK_PREFIX)) {
    throw new Error('email10min: invalid session token');
  }
  let raw: string;
  try {
    raw = Buffer.from(token.slice(TOK_PREFIX.length), 'base64').toString('utf8');
  } catch {
    throw new Error('email10min: invalid session token');
  }
  const o = JSON.parse(raw) as { c?: string; t?: string };
  const c = (o.c ?? '').trim();
  const t = (o.t ?? '').trim();
  if (!c || !t) throw new Error('email10min: invalid session token');
  return { cookie: c, csrf: t };
}

/**
 * 创建 email10min 临时邮箱
 * 访问页面获取 session cookie、CSRF token 和自动分配的邮箱地址
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const res = await fetchWithTimeout(`${BASE_URL}/zh`, {
    method: 'GET',
    headers: BROWSER_HEADERS,
  });

  if (!res.ok) {
    throw new Error(`email10min: 创建邮箱失败 HTTP ${res.status}`);
  }

  let cookie = cookieHeaderFromResponse(res.headers);
  const html = await res.text();
  const csrf = extractCsrfToken(html);

  const msgRes = await fetchWithTimeout(`${BASE_URL}/messages?${Date.now()}`, {
    method: 'POST',
    headers: { ...AJAX_HEADERS, Cookie: cookie },
    body: `_token=${encodeURIComponent(csrf)}&captcha=`,
  });

  if (!msgRes.ok) {
    throw new Error(`email10min: 获取邮箱失败 HTTP ${msgRes.status}`);
  }

  cookie = mergeCookieHeader(cookie, msgRes.headers);
  const data = await msgRes.json() as { mailbox?: string };
  const email = (data.mailbox ?? '').trim();
  if (!email || !email.includes('@')) {
    throw new Error('email10min: 未获取到邮箱地址');
  }

  return {
    channel: CHANNEL,
    email,
    token: encodeToken(cookie, csrf),
  };
}

/**
 * 获取 email10min 邮件列表
 * 使用 session cookie 和 CSRF token 轮询邮件
 */
export async function getEmails(recipientEmail: string, token: string): Promise<Email[]> {
  const { cookie, csrf } = decodeToken(token);
  const ts = Date.now();

  const response = await fetchWithTimeout(`${BASE_URL}/messages?${ts}`, {
    method: 'POST',
    headers: {
      ...AJAX_HEADERS,
      Cookie: cookie,
    },
    body: `_token=${encodeURIComponent(csrf)}&captcha=`,
  });

  if (!response.ok) {
    throw new Error(`email10min: 获取邮件失败 HTTP ${response.status}`);
  }

  const rawText = await response.text();
  let data: { mailbox?: string; messages?: Array<Record<string, unknown>> };
  try {
    data = JSON.parse(rawText);
  } catch {
    throw new Error(`email10min: 返回非 JSON: ${rawText.slice(0, 120)}`);
  }

  const messages = data.messages || [];
  if (!Array.isArray(messages)) {
    return [];
  }

  return messages.map((raw: any, index: number) =>
    normalizeEmail(
      {
        id: raw.id || raw.message_id || String(index),
        from: raw.from || raw.sender || '',
        to: raw.to || recipientEmail,
        subject: raw.subject || '',
        text: raw.text || raw.body || '',
        html: raw.html || raw.body_html || '',
        date: raw.date || raw.created_at || '',
        isRead: false,
        attachments: raw.attachments || [],
      },
      recipientEmail,
    ),
  );
}
