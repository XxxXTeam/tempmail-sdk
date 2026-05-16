import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'minmail';
const BASE_URL = 'https://minmail.app/api';

const REFERER = 'https://minmail.app/';

const DEFAULT_HEADERS = {
  'Accept': '*/*',
  'Accept-Language': 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
  'Origin': 'https://minmail.app',
  'Referer': REFERER,
  'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/148.0.0.0 Safari/537.36 Edg/148.0.0.0',
  'cache-control': 'no-cache',
  'dnt': '1',
  'pragma': 'no-cache',
  'priority': 'u=1, i',
  'sec-ch-ua': '"Chromium";v="148", "Microsoft Edge";v="148", "Not/A)Brand";v="99"',
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
  fromAddress?: string;
  fromName?: string;
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
      map.set(trimmed.slice(0, eq).trim(), trimmed.slice(eq + 1).trim());
    }
  }
  return [...map.entries()].map(([k, v]) => `${k}=${v}`).join('; ');
}

function cookieValue(cookieLine: string, name: string): string {
  for (const pair of cookieLine.split(';')) {
    const trimmed = pair.trim();
    if (!trimmed) continue;
    const eq = trimmed.indexOf('=');
    if (eq <= 0) continue;
    if (trimmed.slice(0, eq).trim() === name) {
      return trimmed.slice(eq + 1).trim();
    }
  }
  return '';
}

/** token：JSON {"visitorId","ck","cookie"} 或旧版仅 UUID；visitorId/ck 亦可来自 Set-Cookie */
function parseMinmailToken(token: string | undefined): { visitorId: string; ck: string; cookie: string } {
  const t = (token ?? '').trim();
  if (t.startsWith('{')) {
    try {
      const o = JSON.parse(t) as { visitorId?: string; visitor_id?: string; ck?: string; cookie?: string };
      const cookie = o.cookie ?? '';
      const vid = o.visitorId ?? o.visitor_id ?? cookieValue(cookie, 'visitorId') ?? '';
      const ck = o.ck ?? cookieValue(cookie, 'ck') ?? '';
      return { visitorId: vid, ck, cookie };
    } catch {
      /* fallthrough */
    }
  }
  return { visitorId: t, ck: '', cookie: '' };
}

function encodeMinmailToken(visitorId: string, ck: string, cookie: string): string {
  return JSON.stringify({ visitorId, ck, cookie });
}

function minmailAddrInHeader(h: string, want: string): boolean {
  if (!h) return true;
  const w = want.trim().toLowerCase();
  const t = h.trim().toLowerCase();
  if (t === w) return true;
  const m = t.match(/<([^>]+@[^>]+)>/);
  if (m && m[1].trim() === w) return true;
  return t.includes(w);
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const visitorId = generateVisitorId();
  const cookieStr = minmailCookie();
  const headers = {
    ...DEFAULT_HEADERS,
    'visitor-id': visitorId,
    'Cookie': cookieStr,
  };

  const response = await fetchWithTimeout(`${BASE_URL}/mail/address?refresh=true&expire=1440&part=main`, {
    method: 'GET',
    headers,
  });

  if (!response.ok) {
    throw new Error(`Failed to generate email: ${response.status}`);
  }

  let cookieStrMerged = mergeCookieHeader(cookieStr, response.headers);
  const data: MinmailAddressResponse = await response.json();
  if (!data.address) {
    throw new Error('minmail: empty address');
  }
  const boundVid =
    data.visitorId ?? data.visitor_id ?? cookieValue(cookieStrMerged, 'visitorId') ?? visitorId;
  const ck = data.ck ?? response.headers.get('ck') ?? cookieValue(cookieStrMerged, 'ck') ?? '';
  if (boundVid && !cookieValue(cookieStrMerged, 'visitorId')) {
    cookieStrMerged = mergeCookieHeader(`${cookieStrMerged}; visitorId=${boundVid}`, response.headers);
  }

  return {
    channel: CHANNEL,
    email: data.address,
    token: encodeMinmailToken(boundVid, ck, cookieStrMerged),
    expiresAt: data.expire ? Date.now() + data.expire * 60 * 1000 : undefined,
  };
}

export async function getEmails(email: string, token?: string): Promise<Email[]> {
  const { visitorId, ck, cookie } = parseMinmailToken(token);
  if (!visitorId) {
    throw new Error('minmail: token must include visitorId');
  }
  const vid = visitorId;

  const headers: Record<string, string> = {
    ...DEFAULT_HEADERS,
    'visitor-id': vid,
    'Cookie': cookie || minmailCookie(),
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
    .filter((m: MinmailMessage) => minmailAddrInHeader(m.to || '', email))
    .map((raw: MinmailMessage) => normalizeEmail({
      id: raw.id,
      from: raw.from || '',
      fromAddress: raw.fromAddress,
      fromName: raw.fromName,
      to: raw.to || email,
      subject: raw.subject || '',
      text: raw.preview || '',
      html: raw.content || '',
      date: raw.date || '',
      isRead: raw.isRead || false,
    }, email));
}