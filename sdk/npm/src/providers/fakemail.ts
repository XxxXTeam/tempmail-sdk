import { setDefaultResultOrder } from 'dns';
import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'fakemail';
const BASE_URL = 'https://www.fakemail.net';
const CSRF_RE = /const\s+CSRF\s*=\s*"([^"]+)"/;

interface GenerateResponse {
  email?: string;
  heslo?: string;
}

interface ListRow {
  id?: string | number;
  predmet?: string;
  od?: string;
  kdy?: string;
  precteno?: string;
}

interface DetailResponse {
  id?: string | number;
  predmet?: string;
  od?: string;
  telo?: string;
  komu?: string;
}

function preferIPv4(): void {
  try {
    setDefaultResultOrder('ipv4first');
  } catch {
    /* Older Node versions may not support this option. */
  }
}

function setCookieLines(headers: Headers): string[] {
  const h = headers as Headers & { getSetCookie?: () => string[] };
  if (typeof h.getSetCookie === 'function') return h.getSetCookie();
  const one = headers.get('set-cookie');
  return one ? one.split(/,(?=[^ ;]+=)/) : [];
}

function mergeCookieHeader(prev: string, headers: Headers): string {
  const jar = new Map<string, string>();
  for (const part of prev.split(';')) {
    const p = part.trim();
    const i = p.indexOf('=');
    if (i > 0) jar.set(p.slice(0, i), p.slice(i + 1));
  }
  for (const line of setCookieLines(headers)) {
    const first = line.split(';')[0]?.trim() || '';
    const i = first.indexOf('=');
    if (i > 0) jar.set(first.slice(0, i), first.slice(i + 1));
  }
  return [...jar.entries()].map(([k, v]) => `${k}=${v}`).join('; ');
}

function parseJSON<T>(text: string): T {
  return JSON.parse(text.replace(/^\uFEFF/, '')) as T;
}

function homeHeaders(): Record<string, string> {
  return {
    Accept: 'text/html,application/xhtml+xml',
    'User-Agent': 'Mozilla/5.0',
  };
}

function ajaxHeaders(cookie: string): Record<string, string> {
  return {
    Accept: 'application/json, text/javascript, */*; q=0.01',
    'X-Requested-With': 'XMLHttpRequest',
    Referer: `${BASE_URL}/`,
    Cookie: cookie,
    'User-Agent': 'Mozilla/5.0',
  };
}

async function openSession(): Promise<{ csrf: string; cookie: string }> {
  preferIPv4();
  const response = await fetchWithTimeout(`${BASE_URL}/`, { headers: homeHeaders() });
  if (!response.ok) throw new Error(`fakemail home: ${response.status}`);
  const cookie = mergeCookieHeader('', response.headers);
  const html = await response.text();
  const csrf = CSRF_RE.exec(html)?.[1] || '';
  if (!csrf) throw new Error('fakemail: csrf token not found');
  return { csrf, cookie };
}

async function createAddress(session: { csrf: string; cookie: string }): Promise<{ data: GenerateResponse; cookie: string }> {
  preferIPv4();
  const response = await fetchWithTimeout(`${BASE_URL}/index/index?csrf_token=${encodeURIComponent(session.csrf)}`, {
    headers: ajaxHeaders(session.cookie),
  });
  if (!response.ok) throw new Error(`fakemail generate: ${response.status}`);
  const cookie = mergeCookieHeader(session.cookie, response.headers);
  const data = parseJSON<GenerateResponse>(await response.text());
  return { data, cookie };
}

async function listRows(cookie: string): Promise<ListRow[]> {
  preferIPv4();
  const response = await fetchWithTimeout(`${BASE_URL}/index/refresh`, {
    headers: ajaxHeaders(cookie),
  });
  if (!response.ok) throw new Error(`fakemail refresh: ${response.status}`);
  const rows = parseJSON<unknown>(await response.text());
  return Array.isArray(rows) ? rows as ListRow[] : [];
}

async function fetchDetail(cookie: string, id: string): Promise<DetailResponse | null> {
  preferIPv4();
  try {
    const response = await fetchWithTimeout(`${BASE_URL}/index/email`, {
      method: 'POST',
      headers: {
        ...ajaxHeaders(cookie),
        'Content-Type': 'application/x-www-form-urlencoded',
      },
      body: new URLSearchParams({ id }).toString(),
    });
    if (!response.ok) return null;
    return parseJSON<DetailResponse>(await response.text());
  } catch {
    return null;
  }
}

function flatten(row: ListRow, detail: DetailResponse | null, recipient: string): Record<string, unknown> {
  return {
    id: detail?.id ?? row.id,
    from: detail?.od || row.od || '',
    to: recipient,
    subject: detail?.predmet || row.predmet || '',
    text: '',
    html: detail?.telo || '',
    date: row.kdy || '',
    isRead: row.precteno === 'precteno',
  };
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const session = await openSession();
  const { data, cookie } = await createAddress(session);
  const email = String(data.email || '').trim();
  if (!email || !email.includes('@')) {
    throw new Error('fakemail: invalid mailbox response');
  }
  return { channel: CHANNEL, email, token: cookie };
}

export async function getEmails(token: string, email: string): Promise<Email[]> {
  const cookie = String(token || '').trim();
  const address = String(email || '').trim();
  if (!cookie) throw new Error('fakemail: empty session token');
  if (!address) throw new Error('fakemail: empty email');
  const rows = await listRows(cookie);
  const emails: Email[] = [];
  for (const row of rows) {
    const id = String(row.id || '').trim();
    const detail = id ? await fetchDetail(cookie, id) : null;
    emails.push(normalizeEmail(flatten(row, detail, address), address));
  }
  return emails;
}
