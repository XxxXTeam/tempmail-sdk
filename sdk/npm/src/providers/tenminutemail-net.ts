/**
 * 10minutemail.net 临时邮箱渠道
 * 网站: 10minutemail.net
 * 基于 Session Cookie 的 HTML 解析方式，域名每 45 天更换一次
 */
import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = '10minutemail-net' as Channel;
const BASE_URL = 'https://10minutemail.net';

/** 公共请求头 */
const COMMON_HEADERS: Record<string, string> = {
  'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
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
 * token 内部存储 JSON: {cookie}
 * 该渠道不需要额外的认证 token，仅使用 session cookie
 */
function encodeToken(cookie: string): string {
  return Buffer.from(JSON.stringify({ cookie }), 'utf8').toString('base64');
}

function decodeToken(token: string): { cookie: string } {
  let raw: string;
  try {
    raw = Buffer.from(token, 'base64').toString('utf8');
  } catch {
    throw new Error('10minutemail-net: token 解码失败');
  }
  const obj = JSON.parse(raw) as { cookie?: string };
  if (!obj.cookie) {
    throw new Error('10minutemail-net: token 缺少必要字段');
  }
  return { cookie: obj.cookie };
}

/**
 * 从首页 HTML 中提取邮箱地址
 * 匹配 <input id="fe_text" value="xxx@yyy.com">
 */
function extractEmailFromHtml(html: string): string {
  const match = html.match(/<input[^>]*\bid\s*=\s*"fe_text"[^>]*\bvalue\s*=\s*"([^"]+)"/i);
  if (match?.[1]) {
    return match[1].trim();
  }
  /* 备用匹配: value 在 id 之前 */
  const alt = html.match(/<input[^>]*\bvalue\s*=\s*"([^"@]+@[^"]+)"[^>]*\bid\s*=\s*"fe_text"/i);
  if (alt?.[1]) {
    return alt[1].trim();
  }
  throw new Error('10minutemail-net: 无法从 HTML 中提取邮箱地址');
}

/**
 * 从邮件列表 HTML 中解析邮件条目
 * 邮件列表为 HTML 表格，每行 <tr> 包含:
 *   <a href="readmail.html?mid=xxx">发件人</a>
 *   <a>主题</a>
 *   <a>日期</a>
 */
function parseMailListHtml(html: string): Array<{ mid: string; from: string; subject: string; date: string }> {
  const results: Array<{ mid: string; from: string; subject: string; date: string }> = [];
  /* 匹配所有 <tr> 行 */
  const trRegex = /<tr[^>]*>([\s\S]*?)<\/tr>/gi;
  let trMatch: RegExpExecArray | null;
  while ((trMatch = trRegex.exec(html)) !== null) {
    const trContent = trMatch[1];
    /* 匹配包含 readmail.html?mid= 的链接 */
    const midMatch = trContent.match(/href\s*=\s*"readmail\.html\?mid=([^"&]+)"/i);
    if (!midMatch?.[1]) continue;
    const mid = midMatch[1];
    /* 提取所有 <a> 标签的文本内容 */
    const anchorTexts: string[] = [];
    const aRegex = /<a[^>]*>([\s\S]*?)<\/a>/gi;
    let aMatch: RegExpExecArray | null;
    while ((aMatch = aRegex.exec(trContent)) !== null) {
      anchorTexts.push(stripHtmlTags(aMatch[1]).trim());
    }
    /* 第一个 <a> 是发件人，第二个是主题，第三个是日期 */
    results.push({
      mid,
      from: anchorTexts[0] || '',
      subject: anchorTexts[1] || '',
      date: anchorTexts[2] || '',
    });
  }
  return results;
}

/**
 * 去除 HTML 标签，保留纯文本
 */
function stripHtmlTags(html: string): string {
  return html.replace(/<[^>]*>/g, '');
}

/**
 * 创建 10minutemail.net 临时邮箱
 *
 * 流程:
 * 1. GET / → 获取 Session Cookie 和邮箱地址
 * 2. 从 HTML 中的 <input id="fe_text"> 提取邮箱地址
 * 3. token 中存储 Session Cookie
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const res = await fetchWithTimeout(BASE_URL, {
    headers: COMMON_HEADERS,
    redirect: 'follow',
  });
  if (!res.ok) {
    throw new Error(`10minutemail-net: 获取首页失败 HTTP ${res.status}`);
  }

  const html = await res.text();
  const cookie = buildCookieHeader('', res.headers);

  if (!cookie) {
    throw new Error('10minutemail-net: 未获取到 session cookie');
  }

  const email = extractEmailFromHtml(html);
  const token = encodeToken(cookie);

  return {
    channel: CHANNEL,
    email,
    token,
  };
}

/**
 * 获取 10minutemail.net 邮箱的邮件列表
 *
 * 流程:
 * 1. GET /mailbox.ajax.php → 获取 HTML 格式的邮件列表
 * 2. 解析 HTML 表格，提取邮件 mid、发件人、主题、日期
 * 3. GET /readmail.html?mid={mid} → 获取邮件 HTML 正文
 */
export async function getEmails(token: string, email: string): Promise<Email[]> {
  if (!token?.trim()) {
    throw new Error('10minutemail-net: token 为空');
  }

  const { cookie } = decodeToken(token);

  /* 获取邮件列表 HTML */
  const listUrl = `${BASE_URL}/mailbox.ajax.php`;
  const listRes = await fetchWithTimeout(listUrl, {
    headers: {
      ...COMMON_HEADERS,
      'Cookie': cookie,
      'X-Requested-With': 'XMLHttpRequest',
    },
  });
  if (!listRes.ok) {
    throw new Error(`10minutemail-net: 获取邮件列表失败 HTTP ${listRes.status}`);
  }

  const listHtml = await listRes.text();
  const trimmed = listHtml.trim();

  /* 空收件箱检查 */
  if (!trimmed || trimmed === '0' || !trimmed.includes('readmail.html')) {
    return [];
  }

  const mailItems = parseMailListHtml(trimmed);
  if (mailItems.length === 0) {
    return [];
  }

  /* 获取每封邮件的详细内容 */
  const emails: Email[] = [];
  for (const item of mailItems) {
    let htmlBody = '';
    try {
      const detailUrl = `${BASE_URL}/readmail.html?mid=${encodeURIComponent(item.mid)}`;
      const detailRes = await fetchWithTimeout(detailUrl, {
        headers: {
          ...COMMON_HEADERS,
          'Cookie': cookie,
        },
      });
      if (detailRes.ok) {
        htmlBody = await detailRes.text();
      }
    } catch {
      /* 获取详情失败时跳过正文 */
    }

    const mailData: Record<string, unknown> = {
      id: item.mid,
      from: item.from,
      to: email,
      subject: item.subject,
      html: htmlBody,
      date: item.date,
      isRead: false,
    };

    emails.push(normalizeEmail(mailData, email));
  }

  return emails;
}
