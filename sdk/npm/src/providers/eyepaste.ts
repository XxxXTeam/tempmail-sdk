import { InternalEmailInfo, Email, Channel } from '../types';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'eyepaste';
const DOMAIN = 'eyepaste.com';
const BASE = 'https://www.eyepaste.com';

const DEFAULT_HEADERS: Record<string, string> = {
  Accept: 'application/rss+xml, application/xml, text/xml, */*',
  'Accept-Language': 'zh-CN,zh;q=0.9,en;q=0.8',
  'Cache-Control': 'no-cache',
  DNT: '1',
  Pragma: 'no-cache',
  Referer: `${BASE}/`,
  'User-Agent':
    'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
};

/**
 * 生成随机用户名
 */
function randomUsername(length = 10): string {
  const chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
  let result = '';
  for (let i = 0; i < length; i++) {
    result += chars.charAt(Math.floor(Math.random() * chars.length));
  }
  return result;
}

/**
 * 创建 eyepaste.com 临时邮箱
 * 无需 API 调用，直接生成随机用户名 + @eyepaste.com
 */
export async function generateEmail(_domain?: string | null, channel: Channel = CHANNEL): Promise<InternalEmailInfo> {
  const username = randomUsername();
  const email = `${username}@${DOMAIN}`;
  return {
    channel,
    email,
  };
}

/**
 * 从 RSS XML 中提取所有 <item> 元素
 */
function extractItems(xml: string): string[] {
  const items: string[] = [];
  const regex = /<item>([\s\S]*?)<\/item>/gi;
  let match: RegExpExecArray | null;
  while ((match = regex.exec(xml)) !== null) {
    items.push(match[1]);
  }
  return items;
}

/**
 * 从 XML 标签中提取文本内容
 */
function extractTag(xml: string, tag: string): string {
  const regex = new RegExp(`<${tag}[^>]*>([\\s\\S]*?)</${tag}>`, 'i');
  const match = xml.match(regex);
  return match ? match[1].trim() : '';
}

/**
 * 解码 XML 实体和 CDATA
 */
function decodeXmlEntities(text: string): string {
  /* 移除 CDATA 包装 */
  let result = text.replace(/<!\[CDATA\[([\s\S]*?)\]\]>/g, '$1');
  /* 解码常见 XML 实体 */
  result = result
    .replace(/&lt;/g, '<')
    .replace(/&gt;/g, '>')
    .replace(/&amp;/g, '&')
    .replace(/&quot;/g, '"')
    .replace(/&#39;/g, "'")
    .replace(/&apos;/g, "'");
  return result;
}

/**
 * 从 description 中解析邮件元数据和正文
 * description 格式:
 *   第一个 <p> 包含 " From: {from} To: {to} Subject: {subject} Date: {date}"
 *   后续内容是邮件正文
 */
function parseDescription(description: string): { from: string; subject: string; date: string; body: string } {
  const decoded = decodeXmlEntities(description);
  let from = '';
  let subject = '';
  let date = '';
  let body = '';

  /* 尝试从第一个 <p> 标签提取元数据 */
  const pMatch = decoded.match(/<p[^>]*>([\s\S]*?)<\/p>/i);
  if (pMatch) {
    const metaText = pMatch[1];
    /* 提取 From */
    const fromMatch = metaText.match(/From:\s*(.*?)(?:\s+To:|$)/i);
    if (fromMatch) from = fromMatch[1].trim();
    /* 提取 Subject */
    const subjectMatch = metaText.match(/Subject:\s*(.*?)(?:\s+Date:|$)/i);
    if (subjectMatch) subject = subjectMatch[1].trim();
    /* 提取 Date */
    const dateMatch = metaText.match(/Date:\s*(.*?)$/i);
    if (dateMatch) date = dateMatch[1].trim();

    /* 正文是 description 中第一个 </p> 之后的内容 */
    const pEnd = decoded.indexOf('</p>');
    if (pEnd !== -1) {
      body = decoded.substring(pEnd + 4).trim();
    }
  } else {
    /* 无 <p> 标签，尝试直接从文本中提取 */
    const fromMatch = decoded.match(/From:\s*(.*?)(?:\s+To:|$)/im);
    if (fromMatch) from = fromMatch[1].trim();
    const subjectMatch = decoded.match(/Subject:\s*(.*?)(?:\s+Date:|$)/im);
    if (subjectMatch) subject = subjectMatch[1].trim();
    const dateMatch = decoded.match(/Date:\s*(.*?)(?:<br|$)/im);
    if (dateMatch) date = dateMatch[1].trim();
    /* 正文在 <br> 之后 */
    const brIndex = decoded.indexOf('<br');
    if (brIndex !== -1) {
      const afterBr = decoded.substring(brIndex);
      const closeBr = afterBr.indexOf('>');
      if (closeBr !== -1) {
        body = afterBr.substring(closeBr + 1).trim();
      }
    }
  }

  return { from, subject, date, body };
}

/**
 * 获取 eyepaste.com 邮件列表
 * GET https://www.eyepaste.com/inbox/{email}.rss
 * 返回 RSS 2.0 XML，需要解析 <item> 中的 <description> 提取邮件数据
 */
export async function getEmails(email: string): Promise<Email[]> {
  if (!email?.trim()) {
    throw new Error('eyepaste: 邮箱地址为空');
  }
  const addr = email.trim();
  const response = await fetchWithTimeout(`${BASE}/inbox/${encodeURIComponent(addr)}.rss`, {
    method: 'GET',
    headers: DEFAULT_HEADERS,
  });
  if (!response.ok) {
    throw new Error(`eyepaste: 获取邮件失败 HTTP ${response.status}`);
  }
  const xml = await response.text();
  const items = extractItems(xml);
  if (items.length === 0) {
    return [];
  }
  return items.map((item, index) => {
    const title = decodeXmlEntities(extractTag(item, 'title'));
    const description = extractTag(item, 'description');
    const parsed = parseDescription(description);
    const html = parsed.body || '';
    /* 从 HTML 中提取纯文本 */
    const text = html.replace(/<[^>]*>/g, '').trim();

    let dateStr = '';
    try {
      if (parsed.date) {
        dateStr = new Date(parsed.date).toISOString();
      }
    } catch {
      /* 日期解析失败 */
    }

    return {
      id: String(index),
      from: parsed.from,
      to: addr,
      subject: parsed.subject || title,
      text,
      html,
      date: dateStr,
      isRead: false,
      attachments: [],
    };
  });
}
