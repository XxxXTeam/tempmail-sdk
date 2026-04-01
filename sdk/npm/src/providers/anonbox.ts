import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'anonbox';
const PAGE_URL = 'https://anonbox.net/en/';
const BASE = 'https://anonbox.net';

const UA =
  'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36';

/**
 * 去掉标签，保留标签内文本（含 display:none 的扰动片段），用于还原完整本地部分
 */
function innerTextFromHtmlFragment(html: string): string {
  return html.replace(/<[^>]+>/g, '').replace(/&nbsp;/g, ' ').replace(/&amp;/g, '&').replace(/&lt;/g, '<').replace(/&gt;/g, '>').trim();
}

/**
 * 从 /en/ 页面解析：收件箱 id、私密路径 token、规范地址、可选过期文案
 */
function parseEnPage(html: string): { inbox: string; token: string; email: string; expiresAt?: string } {
  const mailLink = html.match(/<a href="https:\/\/anonbox\.net\/([^/]+)\/([^"]+)">https:\/\/anonbox\.net\/\1\/\2<\/a>/);
  if (!mailLink) {
    throw new Error('anonbox: 未找到收件箱链接');
  }
  const inbox = mailLink[1];
  const secret = mailLink[2];
  const token = `${inbox}/${secret}`;

  const ddBlocks = [...html.matchAll(/<dd([^>]*)>([\s\S]*?)<\/dd>/gi)];
  let addressHtml: string | null = null;
  for (const m of ddBlocks) {
    const attrs = m[1] || '';
    if (/display\s*:\s*none/i.test(attrs)) continue;
    const inner = m[2];
    const pMatch = inner.match(/<p>([\s\S]*?)<\/p>/i);
    if (!pMatch) continue;
    const pInner = pMatch[1];
    if (!pInner.includes('@')) continue;
    if (/googlemail\.com/i.test(pInner)) continue;
    if (!/anonbox/i.test(pInner)) continue;
    addressHtml = pInner;
    break;
  }
  if (!addressHtml) {
    throw new Error('anonbox: 未解析到有效地址段落');
  }

  const merged = innerTextFromHtmlFragment(addressHtml);
  const at = merged.indexOf('@');
  if (at < 0) {
    throw new Error('anonbox: 地址格式异常');
  }
  const local = merged.slice(0, at).trim();
  if (!local) {
    throw new Error('anonbox: 本地部分为空');
  }
  const email = `${local}@${inbox}.anonbox.net`;

  let expiresAt: string | undefined;
  const expMatch = html.match(/Your mail address is valid until:<\/dt>\s*<dd><p>([^<]+)<\/p>/i);
  if (expMatch) {
    expiresAt = expMatch[1].trim();
  }

  return { inbox, token, email, expiresAt };
}

function simpleHash(s: string): string {
  let h = 0;
  for (let i = 0; i < s.length; i++) {
    h = (Math.imul(31, h) + s.charCodeAt(i)) | 0;
  }
  return Math.abs(h).toString(36);
}

/**
 * 将 mbox 拆成多封 RFC822 片段（首行可能为 "From ..." 信封行）
 */
function splitMbox(raw: string): string[] {
  const t = raw.trim();
  if (!t) return [];
  return t.split(/\r?\n(?=From )/);
}

/**
 * 解析单条 mbox 记录为标准化邮件（正文尽量取 text/plain，否则整段作为 text）
 */
function mboxBlockToEmail(block: string, recipient: string): Email {
  const normalized = block.replace(/\r\n/g, '\n');
  const lines = normalized.split('\n');
  let i = 0;
  if (lines[0].startsWith('From ')) {
    i = 1;
  }
  const headers: Record<string, string> = {};
  let curKey = '';
  for (; i < lines.length; i++) {
    const line = lines[i];
    if (line === '') {
      i++;
      break;
    }
    if (/^[ \t]/.test(line) && curKey) {
      headers[curKey] += ' ' + line.trim();
    } else {
      const hm = line.match(/^([^:]+):\s*(.*)$/);
      if (hm) {
        curKey = hm[1].trim().toLowerCase();
        headers[curKey] = hm[2];
      }
    }
  }
  const body = lines.slice(i).join('\n');

  const ct = (headers['content-type'] || '').toLowerCase();
  let text = '';
  let html = '';
  if (ct.includes('multipart/')) {
    const boundaryM = headers['content-type'].match(/boundary="?([^";\s]+)"?/i);
    const boundary = boundaryM ? boundaryM[1] : '';
    if (boundary) {
      const parts = body.split(new RegExp(`\\r?\\n--${boundary.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}(?:--)?\\r?\\n`));
      for (const part of parts) {
        const p = part.trim();
        if (!p || p === '--') continue;
        const sep = p.indexOf('\n\n');
        if (sep < 0) continue;
        const ph = p.slice(0, sep);
        const pb = p.slice(sep + 2);
        const pct = (ph.match(/^content-type:\s*([^\s;]+)/im) || [])[1]?.toLowerCase() || '';
        if (pct === 'text/plain') {
          text = decodeQuotedPrintableIfNeeded(pb, ph);
        } else if (pct === 'text/html') {
          html = decodeQuotedPrintableIfNeeded(pb, ph);
        }
      }
    }
  }
  if (!text && !html) {
    if (ct.includes('text/html')) {
      html = decodeQuotedPrintableIfNeeded(body, headers['content-type'] || '');
    } else {
      text = decodeQuotedPrintableIfNeeded(body, headers['content-type'] || '');
    }
  }

  const from = headers['from'] || '';
  const to = headers['to'] || recipient;
  const subject = headers['subject'] || '';
  const date = headers['date'] ? new Date(headers['date']).toISOString() : new Date().toISOString();
  const id = headers['message-id'] || simpleHash(block.slice(0, 512));

  return normalizeEmail(
    {
      id,
      from,
      to,
      subject,
      text,
      html,
      date,
      isRead: false,
      attachments: [],
    },
    recipient,
  );
}

function decodeQuotedPrintableIfNeeded(body: string, headerBlock: string): string {
  const te = (headerBlock.match(/^content-transfer-encoding:\s*([^\s]+)/im) || [])[1]?.toLowerCase() || '';
  if (te !== 'quoted-printable') {
    return body.trimEnd();
  }
  const soft = body.replace(/=\r?\n/g, '');
  return soft
    .replace(/=([0-9A-F]{2})/gi, (_, h) => String.fromCharCode(parseInt(h, 16)))
    .trimEnd();
}

/**
 * 创建 anonbox 一次性邮箱：GET 英文页并解析 HTML
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const response = await fetchWithTimeout(PAGE_URL, {
    headers: { 'User-Agent': UA, Accept: 'text/html,application/xhtml+xml' },
  });
  if (!response.ok) {
    throw new Error(`anonbox: 创建失败 HTTP ${response.status}`);
  }
  const html = await response.text();
  const parsed = parseEnPage(html);
  return {
    channel: CHANNEL,
    email: parsed.email,
    token: parsed.token,
    expiresAt: parsed.expiresAt,
  };
}

/**
 * 拉取 mbox 明文并拆成邮件列表（空信箱为 0 长度正文）
 */
export async function getEmails(token: string, email: string): Promise<Email[]> {
  if (!token) {
    throw new Error('internal error: token missing for anonbox');
  }
  const path = token.endsWith('/') ? token : `${token}/`;
  const url = `${BASE}/${path}`;
  const response = await fetchWithTimeout(url, {
    headers: { 'User-Agent': UA, Accept: 'text/plain,*/*' },
  });
  if (response.status === 404) {
    return [];
  }
  if (!response.ok) {
    throw new Error(`anonbox: 获取邮件失败 HTTP ${response.status}`);
  }
  const raw = await response.text();
  const blocks = splitMbox(raw);
  return blocks.map((b) => mboxBlockToEmail(b, email));
}
