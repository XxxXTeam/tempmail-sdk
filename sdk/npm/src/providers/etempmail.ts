import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'etempmail';
const BASE = 'https://etempmail.com';

const BROWSER_HEADERS: Record<string, string> = {
  Accept: '*/*',
  'Accept-Language': 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
  'Cache-Control': 'no-cache',
  DNT: '1',
  Origin: BASE,
  Pragma: 'no-cache',
  Referer: `${BASE}/zh`,
  'sec-ch-ua': '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
  'sec-ch-ua-mobile': '?0',
  'sec-ch-ua-platform': '"Windows"',
  'sec-fetch-dest': 'empty',
  'sec-fetch-mode': 'cors',
  'sec-fetch-site': 'same-origin',
  'User-Agent':
    'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
  'X-Requested-With': 'XMLHttpRequest',
};

/** 从响应头提取 `name=value` Cookie 串（支持多条 Set-Cookie） */
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
      const name = trimmed.slice(0, eq).trim();
      const value = trimmed.slice(eq + 1).trim();
      map.set(name, value);
    }
  }
  return [...map.entries()].map(([k, v]) => `${k}=${v}`).join('; ');
}

async function establishSession(): Promise<string> {
  const res = await fetchWithTimeout(`${BASE}/zh`, {
    method: 'GET',
    headers: BROWSER_HEADERS,
  });
  if (!res.ok) {
    throw new Error(`etempmail: session page failed: ${res.status}`);
  }
  const cookie = cookieHeaderFromResponse(res.headers);
  if (!cookie.includes('ci_session=')) {
    throw new Error('etempmail: no ci_session cookie from server');
  }
  return cookie;
}

interface GetEmailAddressResponse {
  id?: string;
  address?: string;
  creation_time?: string;
  recover_key?: string;
}

interface InboxItem {
  subject?: string;
  from?: string;
  date?: string;
  body?: string;
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  let cookie = await establishSession();
  const res = await fetchWithTimeout(`${BASE}/getEmailAddress`, {
    method: 'POST',
    headers: {
      ...BROWSER_HEADERS,
      Cookie: cookie,
    },
  });
  cookie = mergeCookieHeader(cookie, res.headers);
  const rawText = await res.text();
  if (!res.ok) {
    throw new Error(`etempmail: getEmailAddress failed: ${res.status}`);
  }
  let data: GetEmailAddressResponse;
  try {
    data = JSON.parse(rawText) as GetEmailAddressResponse;
  } catch {
    throw new Error(`etempmail: getEmailAddress returned non-JSON: ${rawText.slice(0, 120)}`);
  }
  if (!data.address) {
    throw new Error('etempmail: no address in response');
  }
  const creationSec = data.creation_time ? parseInt(data.creation_time, 10) : NaN;
  return {
    channel: CHANNEL,
    email: data.address,
    token: cookie,
    createdAt: !Number.isNaN(creationSec) ? new Date(creationSec * 1000).toISOString() : undefined,
  };
}

export async function getEmails(recipientEmail: string, cookie: string): Promise<Email[]> {
  const res = await fetchWithTimeout(`${BASE}/getInbox`, {
    method: 'POST',
    headers: {
      ...BROWSER_HEADERS,
      Cookie: cookie,
    },
  });
  const rawText = await res.text();
  if (!res.ok) {
    throw new Error(`etempmail: getInbox failed: ${res.status}`);
  }
  let items: InboxItem[];
  try {
    items = JSON.parse(rawText) as InboxItem[];
  } catch {
    throw new Error(`etempmail: getInbox returned non-JSON: ${rawText.slice(0, 120)}`);
  }
  if (!Array.isArray(items)) {
    throw new Error('etempmail: getInbox expected JSON array');
  }
  return items.map((raw, index) =>
    normalizeEmail(
      {
        id: [raw.from, raw.subject, raw.date, String(index), recipientEmail].join('\n'),
        from: raw.from ?? '',
        subject: raw.subject ?? '',
        body: raw.body ?? '',
        date: raw.date,
        isRead: false,
      },
      recipientEmail,
    ),
  );
}
