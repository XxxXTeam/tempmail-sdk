import { InternalEmailInfo, Email, EmailAttachment, Channel } from '../types';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'linshiyou';
const ORIGIN = 'https://linshiyou.com';

const DEFAULT_HEADERS: Record<string, string> = {
  'User-Agent':
    'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
  'accept-language': 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
  'cache-control': 'no-cache',
  dnt: '1',
  pragma: 'no-cache',
  priority: 'u=1, i',
  referer: `${ORIGIN}/`,
  'sec-ch-ua': '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
  'sec-ch-ua-mobile': '?0',
  'sec-ch-ua-platform': '"Windows"',
  'sec-fetch-dest': 'empty',
  'sec-fetch-mode': 'cors',
  'sec-fetch-site': 'same-origin',
};

function getSetCookieLines(response: Response): string[] {
  const h = response.headers as Headers & { getSetCookie?: () => string[] };
  if (typeof h.getSetCookie === 'function') {
    return h.getSetCookie();
  }
  const single = response.headers.get('set-cookie');
  return single ? [single] : [];
}

function extractNexusToken(setCookieLines: string[]): string {
  for (const line of setCookieLines) {
    const m = line.match(/^\s*NEXUS_TOKEN=([^;]+)/i);
    if (m) return m[1].trim();
  }
  return '';
}

function buildMailCookie(nexusToken: string, email: string): string {
  return `NEXUS_TOKEN=${nexusToken}; tmail-emails=${email}`;
}

function decodeHtmlEntities(s: string): string {
  return s
    .replace(/&#x([0-9a-fA-F]+);/g, (_, h) => String.fromCharCode(parseInt(h, 16)))
    .replace(/&#(\d+);/g, (_, n) => String.fromCharCode(Number(n)))
    .replace(/&quot;/g, '"')
    .replace(/&lt;/g, '<')
    .replace(/&gt;/g, '>')
    .replace(/&amp;/g, '&');
}

function stripTagsToText(html: string): string {
  return decodeHtmlEntities(html.replace(/<[^>]+>/g, ' ')).replace(/\s+/g, ' ').trim();
}

function pickDivClass(html: string, className: string): string {
  const re = new RegExp(`<div class="${className}"[^>]*>([\\s\\S]*?)</div>`, 'i');
  const m = html.match(re);
  return m ? stripTagsToText(m[1]) : '';
}

function parseLinshiyouDate(s: string): string {
  const t = s.trim();
  if (!t) return '';
  try {
    const isoish = t.includes('T') ? t : t.replace(' ', 'T');
    const d = new Date(isoish);
    if (!Number.isNaN(d.getTime())) return d.toISOString();
  } catch {
    /* ignore */
  }
  return '';
}

function extractIframeSrcdoc(html: string): string {
  const m = html.match(/\ssrcdoc="([^"]*)"/i);
  if (!m) return '';
  return decodeHtmlEntities(m[1]);
}

function extractDownloadPath(bodyPart: string): string | null {
  const m = bodyPart.match(/href="(\/api\/download\?id=[^"]+)"/i);
  return m ? m[1] : null;
}

function parseMailSegments(raw: string, recipientEmail: string): Email[] {
  const text = String(raw || '').trim();
  if (!text) return [];

  const segments = text
    .split('<-----TMAILNEXTMAIL----->')
    .map(s => s.trim())
    .filter(Boolean);

  const out: Email[] = [];
  for (const seg of segments) {
    const [listPart = '', bodyPart = ''] = seg.split('<-----TMAILCHOPPER----->');
    const idMatch = listPart.match(/id="tmail-email-list-([a-f0-9]+)"/i);
    const id = idMatch ? idMatch[1] : '';
    if (!id) continue;

    const fromList = pickDivClass(listPart, 'name');
    const subjectList = pickDivClass(listPart, 'subject');
    const previewList = pickDivClass(listPart, 'body');

    const fromBody = pickDivClass(bodyPart, 'tmail-email-sender');
    const timeBody = pickDivClass(bodyPart, 'tmail-email-time');
    const titleBody = pickDivClass(bodyPart, 'tmail-email-title');
    const html = extractIframeSrcdoc(bodyPart);

    const from = fromBody || fromList;
    const subject = titleBody || subjectList;
    const textContent = previewList || stripTagsToText(html);
    const date = parseLinshiyouDate(timeBody);

    const attachments: EmailAttachment[] = [];
    const dl = extractDownloadPath(bodyPart);
    if (dl) {
      attachments.push({
        filename: '',
        url: `${ORIGIN}${dl}`,
      });
    }

    out.push({
      id,
      from,
      to: recipientEmail,
      subject,
      text: textContent,
      html,
      date,
      isRead: false,
      attachments,
    });
  }
  return out;
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const response = await fetchWithTimeout(`${ORIGIN}/api/user?user`, {
    method: 'GET',
    headers: DEFAULT_HEADERS,
  });

  if (!response.ok) {
    throw new Error(`Failed to generate email: ${response.status}`);
  }

  const nexus = extractNexusToken(getSetCookieLines(response));
  const email = (await response.text()).trim();

  if (!nexus) {
    throw new Error('linshiyou: 响应中未找到 NEXUS_TOKEN（Set-Cookie）');
  }
  if (!email || !email.includes('@')) {
    throw new Error('linshiyou: 响应正文未返回有效邮箱地址');
  }

  return {
    channel: CHANNEL,
    email,
    token: buildMailCookie(nexus, email),
  };
}

export async function getEmails(cookieHeader: string, email: string): Promise<Email[]> {
  const response = await fetchWithTimeout(`${ORIGIN}/api/mail?unseen=1`, {
    method: 'GET',
    headers: {
      ...DEFAULT_HEADERS,
      Cookie: cookieHeader,
      'x-requested-with': 'XMLHttpRequest',
    },
  });

  if (!response.ok) {
    throw new Error(`Failed to get emails: ${response.status}`);
  }

  const raw = await response.text();
  return parseMailSegments(raw, email);
}
