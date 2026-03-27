import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'minmail';
const BASE_URL = 'https://minmail.app/api';

const DEFAULT_HEADERS = {
  'Accept': '*/*',
  'Accept-Language': 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
  'Origin': 'https://minmail.app',
  'Referer': 'https://minmail.app/cn',
  'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
  'cache-control': 'no-cache',
  'dnt': '1',
  'pragma': 'no-cache',
  'priority': 'u=1, i',
  'sec-ch-ua': '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
  'sec-ch-ua-mobile': '?0',
  'sec-ch-ua-platform': '"Windows"',
  'sec-fetch-dest': 'empty',
  'sec-fetch-mode': 'cors',
  'sec-fetch-site': 'same-origin',
};

interface MinmailAddressResponse {
  address: string;
  expire: number;
  remainingTime?: number;
  visitorId?: string;
  visitor_id?: string;
  ck?: string;
}

interface MinmailMessage {
  id: string;
  from: string;
  to: string;
  subject: string;
  preview: string;
  content: string;
  date: string;
  isRead: boolean;
}

interface MinmailListResponse {
  message: MinmailMessage[];
}

function randomString(length: number): string {
  const chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
  let result = '';
  for (let i = 0; i < length; i++) {
    result += chars[Math.floor(Math.random() * chars.length)];
  }
  return result;
}

function generateVisitorId(): string {
  return `${randomString(8)}-${randomString(4)}-${randomString(4)}-${randomString(4)}-${randomString(12)}`;
}

function minmailCookie(): string {
  const now = Date.now();
  const ga = `GA1.1.${now}.${Math.floor(Math.random() * 1000000)}`;
  return `_ga=${ga}; _ga_DFGB8WF1WG=GS2.1.s${now}$o1$g0$t${now}$j60$l0$h0`;
}

/** token：JSON {"visitorId","ck"} 或旧版仅 UUID */
function parseMinmailToken(token: string | undefined): { visitorId: string; ck: string } {
  const t = (token ?? '').trim();
  if (t.startsWith('{')) {
    try {
      const o = JSON.parse(t) as { visitorId?: string; visitor_id?: string; ck?: string };
      const vid = o.visitorId ?? o.visitor_id ?? '';
      return { visitorId: vid, ck: o.ck ?? '' };
    } catch {
      /* fallthrough */
    }
  }
  return { visitorId: t, ck: '' };
}

function encodeMinmailToken(visitorId: string, ck: string): string {
  return JSON.stringify({ visitorId, ck });
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const headers = {
    ...DEFAULT_HEADERS,
    'Cookie': minmailCookie(),
  };

  const response = await fetchWithTimeout(`${BASE_URL}/mail/address?refresh=true&expire=1440&part=main`, {
    method: 'GET',
    headers,
  });

  if (!response.ok) {
    throw new Error(`Failed to generate email: ${response.status}`);
  }

  const data: MinmailAddressResponse = await response.json();
  const serverVid = data.visitorId ?? data.visitor_id ?? '';
  const visitorId = serverVid || generateVisitorId();
  const ck = data.ck ?? '';

  return {
    channel: CHANNEL,
    email: data.address,
    token: encodeMinmailToken(visitorId, ck),
    expiresAt: Date.now() + (data.expire * 60 * 1000),
  };
}

export async function getEmails(email: string, token?: string): Promise<Email[]> {
  const { visitorId, ck } = parseMinmailToken(token);
  const vid = visitorId || generateVisitorId();

  const headers: Record<string, string> = {
    ...DEFAULT_HEADERS,
    'visitor-id': vid,
    'Cookie': minmailCookie(),
  };
  if (ck) headers['ck'] = ck;

  const response = await fetchWithTimeout(`${BASE_URL}/mail/list?part=main`, {
    method: 'GET',
    headers,
  });

  if (!response.ok) {
    throw new Error(`Failed to get emails: ${response.status}`);
  }

  const data: MinmailListResponse = await response.json();
  const messages = data.message || [];

  return messages
    .filter((m: MinmailMessage) => m.to === email)
    .map((raw: MinmailMessage) => normalizeEmail({
      id: raw.id,
      from: raw.from || '',
      to: raw.to || email,
      subject: raw.subject || '',
      text: raw.preview || '',
      html: raw.content || '',
      date: raw.date || '',
      isRead: raw.isRead || false,
    }, email));
}