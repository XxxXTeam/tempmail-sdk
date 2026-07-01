/**
 * 10minutemail.net 临时邮箱渠道
 *
 * 流程：
 *   1. GET / 获取 session cookie（PHPSESSID）+ 从 HTML 提取随机分配的邮箱地址
 *   2. GET /mailbox.ajax.php?_={毫秒时间戳} 获取邮件列表（返回 HTML 表格，非 JSON）
 *   3. GET /readmail.html?mid={id} 获取单封邮件整页 HTML，从中提取正文片段
 *
 * Go 端依赖全局 cookie jar 维持 session（token 为空）；本端无 cookie jar，
 * 故将创建时获得的会话 Cookie 串存入 token，getEmails 时手动携带以绑定收件箱。
 */
import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = '10minutemail-net';
const BASE_URL = 'https://10minutemail.net';

const USER_AGENT =
  'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0';

/** 从首页 HTML 的邮箱输入框提取地址（value="xxx@xxx.com"） */
const EMAIL_RE = /value="([^"]+@[^"]+)"/;
/** 从邮件列表表格逐行匹配 <tr>...</tr> */
const ROW_RE = /<tr[^>]*>([\s\S]*?)<\/tr>/gi;
/** 从行内链接提取邮件 ID（readmail.html?mid=xxx） */
const MID_RE = /readmail\.html\?mid=([^'"&]+)/i;
/** 逐个匹配行内 <td>...</td> 单元格 */
const CELL_RE = /<td[^>]*>([\s\S]*?)<\/td>/gi;
/** 从单元格提取 span 的 title 属性（收件时间，UTC 绝对时间） */
const TITLE_RE = /title="([^"]+)"/i;
/** 提取 Cloudflare 邮箱保护混淆数据（data-cfemail="十六进制"） */
const CF_RE = /data-cfemail="([0-9a-fA-F]+)"/i;
/** 去除 HTML 标签用于提取纯文本 */
const TAG_RE = /<[^>]+>/g;

function browserHeaders(): Record<string, string> {
  return {
    'User-Agent': USER_AGENT,
    Accept:
      'text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8',
    'Accept-Language': 'en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7',
  };
}

function ajaxHeaders(): Record<string, string> {
  return {
    'User-Agent': USER_AGENT,
    Accept: '*/*',
    'Accept-Language': 'en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7',
    'X-Requested-With': 'XMLHttpRequest',
    Referer: `${BASE_URL}/`,
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
 * 创建 10minutemail.net 临时邮箱
 * GET / 获取 session cookie 并从首页 HTML 提取随机分配的邮箱地址。
 * token 存储会话 Cookie 串（绑定该收件箱）。
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const r = await fetchWithTimeout(`${BASE_URL}/`, { headers: browserHeaders() });
  if (!r.ok) {
    throw new Error(`10minutemail-net: 获取首页失败 HTTP ${r.status}`);
  }

  const cookieHdr = mergeCookieHeader('', r.headers);
  const html = await r.text();

  const m = EMAIL_RE.exec(html);
  if (!m?.[1]) {
    throw new Error('10minutemail-net: 未能从首页提取邮箱地址');
  }
  const email = m[1].trim();
  if (!email || !email.includes('@')) {
    throw new Error(`10minutemail-net: 获取到的邮箱地址无效: ${email}`);
  }

  return { channel: CHANNEL, email, token: cookieHdr };
}

/**
 * 获取 10minutemail.net 邮件列表
 * 1. GET /mailbox.ajax.php?_={毫秒时间戳} 获取邮件列表（HTML 表格，需带创建时的 PHPSESSID cookie）
 * 2. 逐行解析 <tr>，提取邮件 ID（mid）、发件人、主题、收件时间、已读状态
 * 3. 对每封邮件 GET /readmail.html?mid={id} 获取整页并提取正文 HTML 片段
 * 表格列顺序: 寄件人 | 主题 | 收件日期；未读行的 <tr> 带 font-weight: bold 样式
 */
export async function getEmails(token: string, email: string): Promise<Email[]> {
  const addr = email.trim();
  if (!addr) {
    throw new Error('10minutemail-net: 邮箱地址为空');
  }
  const cookieHdr = (token ?? '').trim();

  const listURL = `${BASE_URL}/mailbox.ajax.php?_=${Date.now()}`;
  const r = await fetchWithTimeout(listURL, {
    headers: { ...ajaxHeaders(), ...(cookieHdr ? { Cookie: cookieHdr } : {}) },
  });
  if (!r.ok) {
    throw new Error(`10minutemail-net: 获取邮件列表失败 HTTP ${r.status}`);
  }

  const body = await r.text();
  const emails: Email[] = [];

  ROW_RE.lastIndex = 0;
  let rowMatch: RegExpExecArray | null;
  while ((rowMatch = ROW_RE.exec(body)) !== null) {
    const rowFull = rowMatch[0];
    const rowInner = rowMatch[1];

    /* 跳过表头行（含 <th>） */
    if (rowInner.toLowerCase().includes('<th')) {
      continue;
    }

    /* 提取邮件 ID */
    const midMatch = MID_RE.exec(rowInner);
    if (!midMatch?.[1]) {
      continue;
    }
    const mailID = midMatch[1].trim();
    if (!mailID) {
      continue;
    }

    /* 提取三个单元格：发件人 | 主题 | 收件日期 */
    const cells: string[] = [];
    CELL_RE.lastIndex = 0;
    let cellMatch: RegExpExecArray | null;
    while ((cellMatch = CELL_RE.exec(rowInner)) !== null) {
      cells.push(cellMatch[1]);
    }

    const fromAddr = extractText(cells[0] ?? '');
    const subject = extractText(cells[1] ?? '');

    /* 收件时间优先取 span 的 title 属性（UTC 绝对时间），否则取单元格文本 */
    const dateCell = cells[2] ?? '';
    const titleMatch = TITLE_RE.exec(dateCell);
    const date = titleMatch?.[1] ? titleMatch[1].trim() : extractText(dateCell);

    /* 未读状态：行 <tr> 样式含 font-weight: bold */
    const isRead = !rowFull.toLowerCase().includes('font-weight: bold');

    const htmlBody = await fetchBody(cookieHdr, mailID);

    const raw: Record<string, unknown> = {
      id: mailID,
      from: fromAddr,
      to: addr,
      subject,
      html: htmlBody,
      date,
      isRead,
    };
    emails.push(normalizeEmail(raw, addr));
  }

  return emails;
}

/**
 * 获取单封邮件的正文 HTML 片段
 * GET /readmail.html?mid={id} 返回整页 HTML，正文位于 <div class="mailinhtml">...</div>
 */
async function fetchBody(cookieHdr: string, mailID: string): Promise<string> {
  if (!mailID) {
    return '';
  }
  const r = await fetchWithTimeout(`${BASE_URL}/readmail.html?mid=${mailID}`, {
    headers: { ...browserHeaders(), Referer: `${BASE_URL}/`, ...(cookieHdr ? { Cookie: cookieHdr } : {}) },
  });
  if (!r.ok) {
    return '';
  }
  return extractBody(await r.text());
}

/**
 * 从整页 HTML 提取正文片段
 * 正文包裹于 <div class="mailinhtml"> ... </div>，内部存在嵌套 div，
 * 故以紧随其后的 email-decode.min.js script 标签作为结束锚点，再回退裁掉尾部的两个闭合 </div>。
 */
function extractBody(page: string): string {
  const startMark = 'class="mailinhtml"';
  const si = page.indexOf(startMark);
  if (si < 0) {
    return '';
  }
  /* 跳到 mailinhtml 这个 div 的 '>' 之后 */
  const gt = page.indexOf('>', si);
  if (gt < 0) {
    return '';
  }
  const rest = page.slice(gt + 1);

  /* 结束锚点：mailinhtml 区块后紧跟的 cloudflare email-decode 脚本 */
  const ei = rest.indexOf('email-decode.min.js');
  if (ei < 0) {
    /* 兜底：退化为该 div 后第一个 </div> */
    const di = rest.indexOf('</div>');
    return di < 0 ? rest.trim() : rest.slice(0, di).trim();
  }

  let segment = rest.slice(0, ei);
  /* segment 末尾形如 "...正文</div></div><script ..."，裁掉结尾的 <script 起始 */
  const sIdx = segment.lastIndexOf('<script');
  if (sIdx >= 0) {
    segment = segment.slice(0, sIdx);
  }
  segment = segment.trim();
  /* 去掉 mailinhtml 与其外层 tab1 的两个闭合 div */
  for (let i = 0; i < 2; i++) {
    segment = segment.trim();
    if (segment.endsWith('</div>')) {
      segment = segment.slice(0, -'</div>'.length);
    }
  }
  return segment.trim();
}

/**
 * 从单元格 HTML 提取纯文本发件人/主题
 * 优先解码 Cloudflare 邮箱保护混淆（__cf_email__ + data-cfemail），否则去标签取文本。
 */
function extractText(cell: string): string {
  const cf = CF_RE.exec(cell);
  if (cf?.[1]) {
    const decoded = cfDecode(cf[1]);
    if (decoded) {
      return decoded;
    }
  }
  let text = cell.replace(TAG_RE, '');
  text = text
    .replace(/&nbsp;/g, ' ')
    .replace(/&#160;/g, ' ')
    .replace(/&amp;/g, '&')
    .replace(/&lt;/g, '<')
    .replace(/&gt;/g, '>')
    .replace(/&quot;/g, '"');
  return text.trim();
}

/**
 * 解码 Cloudflare 邮箱保护混淆字符串
 * 算法：首字节为异或密钥，其后每字节与密钥异或还原 ASCII。
 */
function cfDecode(encoded: string): string {
  if (encoded.length < 4 || encoded.length % 2 !== 0) {
    return '';
  }
  const bytes: number[] = [];
  for (let i = 0; i < encoded.length; i += 2) {
    const b = parseInt(encoded.slice(i, i + 2), 16);
    if (Number.isNaN(b)) {
      return '';
    }
    bytes.push(b);
  }
  const key = bytes[0];
  let decoded = '';
  for (let i = 1; i < bytes.length; i++) {
    decoded += String.fromCharCode(bytes[i] ^ key);
  }
  return decoded.includes('@') ? decoded : '';
}
