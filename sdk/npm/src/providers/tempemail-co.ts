/**
 * TempEmail.co 临时邮箱渠道
 * 网站: tempemail.co
 * Cookie Session + REST JSON API
 *
 * 邮件列表接口返回 HTML 包裹在 JSON 中，需要正则提取邮件 ID 后逐条获取详情
 */
import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'tempemail-co';
const BASE_URL = 'https://tempemail.co';

/** 从 HTML 中提取邮件 ID 的正则 */
const MAIL_ID_RE = /data-id="(\d+)"/g;

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
 * token 内部存储 JSON: {address, cookie}
 */
function encodeToken(address: string, cookie: string): string {
  return Buffer.from(JSON.stringify({ address, cookie }), 'utf8').toString('base64');
}

function decodeToken(token: string): { address: string; cookie: string } {
  let raw: string;
  try {
    raw = Buffer.from(token, 'base64').toString('utf8');
  } catch {
    throw new Error('tempemail-co: token 解码失败');
  }
  const obj = JSON.parse(raw) as { address?: string; cookie?: string };
  if (!obj.address || !obj.cookie) {
    throw new Error('tempemail-co: token 缺少必要字段');
  }
  return { address: obj.address, cookie: obj.cookie };
}

/**
 * 创建 TempEmail.co 临时邮箱
 *
 * 流程:
 * 1. GET /mail/random → 获取随机邮箱地址，同时保存 Cookie
 * 2. 返回: {result: true, id: "user@domain", address: "user@domain"}
 * 3. token 中存储 address 和 Cookie
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const r = await fetchWithTimeout(`${BASE_URL}/mail/random`, {
    headers: COMMON_HEADERS,
    redirect: 'follow',
  });
  if (!r.ok) {
    throw new Error(`tempemail-co: 创建邮箱失败 HTTP ${r.status}`);
  }

  const cookie = buildCookieHeader('', r.headers);
  const data = await r.json() as { result?: boolean; id?: string; address?: string };

  if (!data.result || !data.address) {
    throw new Error('tempemail-co: 创建邮箱返回数据缺少 address 字段');
  }

  const address = data.address;
  const token = encodeToken(address, cookie);

  return {
    channel: CHANNEL,
    email: address,
    token,
  };
}

/**
 * 获取 TempEmail.co 邮箱的邮件列表
 *
 * 流程:
 * 1. GET /get-mails?mail_id={address}&unseen=0&is_new=1 → 邮件列表（HTML 包裹在 JSON 中）
 *    - count == 0 时无邮件
 *    - count > 0 时从 mails HTML 中正则提取 data-id="{id}"
 * 2. GET /mail/info?id={id} → 每封邮件的纯 JSON 详情
 *    - 返回: {result: true, mail: {fromName, fromAddress, displayDate, subject, textHtml}}
 */
export async function getEmails(token: string, email: string): Promise<Email[]> {
  if (!token?.trim()) {
    throw new Error('tempemail-co: token 为空');
  }

  const { address, cookie } = decodeToken(token);

  /* 步骤 1: 获取邮件列表 */
  const listUrl = `${BASE_URL}/get-mails?mail_id=${encodeURIComponent(address)}&unseen=0&is_new=1`;
  const r = await fetchWithTimeout(listUrl, {
    headers: {
      ...COMMON_HEADERS,
      'Cookie': cookie,
      'X-Requested-With': 'XMLHttpRequest',
    },
  });
  if (!r.ok) {
    throw new Error(`tempemail-co: 获取邮件列表失败 HTTP ${r.status}`);
  }

  const data = await r.json() as { result?: boolean; mails?: string; count?: number };
  if (!data.result || !data.count || data.count <= 0) {
    return [];
  }

  /* 从 HTML 中提取邮件 ID */
  const mailsHtml = data.mails || '';
  const ids: string[] = [];
  let match: RegExpExecArray | null;
  while ((match = MAIL_ID_RE.exec(mailsHtml)) !== null) {
    ids.push(match[1]);
  }

  if (ids.length === 0) {
    return [];
  }

  /* 步骤 2: 逐条获取邮件详情 */
  const emails: Email[] = [];
  for (const id of ids) {
    const detailUrl = `${BASE_URL}/mail/info?id=${encodeURIComponent(id)}`;
    const rd = await fetchWithTimeout(detailUrl, {
      headers: {
        ...COMMON_HEADERS,
        'Cookie': cookie,
        'X-Requested-With': 'XMLHttpRequest',
      },
    });
    if (!rd.ok) continue;

    const detail = await rd.json() as {
      result?: boolean;
      mail?: {
        fromName?: string;
        fromAddress?: string;
        displayDate?: string;
        subject?: string;
        textHtml?: string;
      };
    };
    if (!detail.result || !detail.mail) continue;

    const m = detail.mail;
    const mailData: Record<string, unknown> = {
      id: String(id),
      from: m.fromAddress || m.fromName || '',
      to: email,
      subject: m.subject || '',
      html: m.textHtml || '',
      date: m.displayDate || '',
    };

    emails.push(normalizeEmail(mailData, email));
  }

  return emails;
}
