import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = '1sec-mail';
const BASE_URL = 'https://1sec-mail.com/';
const CSRF_RE = /<meta name="csrf-token" content="([^"]+)"/i;

interface OneSecMailSession {
  csrf: string;
  cookie: string;
}

interface MessagesResponse {
  status?: boolean;
  mailbox?: string;
  email_token?: string;
  messages?: Array<Record<string, unknown>>;
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

function encodeSession(session: OneSecMailSession): string {
  return JSON.stringify(session);
}

function decodeSession(token: string): OneSecMailSession {
  const data = JSON.parse(token) as Partial<OneSecMailSession>;
  const csrf = String(data.csrf || '').trim();
  const cookie = String(data.cookie || '').trim();
  if (!csrf || !cookie) throw new Error('1sec-mail: invalid session token');
  return { csrf, cookie };
}

function postHeaders(session: OneSecMailSession): Record<string, string> {
  return {
    Accept: 'application/json, text/plain, */*',
    'Content-Type': 'application/json',
    'X-Requested-With': 'XMLHttpRequest',
    'X-CSRF-TOKEN': session.csrf,
    Referer: BASE_URL,
    Cookie: session.cookie,
    'User-Agent': 'Mozilla/5.0',
  };
}

async function openSession(): Promise<OneSecMailSession> {
  const home = await fetchWithTimeout(BASE_URL, {
    headers: {
      Accept: 'text/html,application/xhtml+xml',
      'User-Agent': 'Mozilla/5.0',
    },
  });
  if (!home.ok) throw new Error(`1sec-mail home: ${home.status}`);
  const cookie = mergeCookieHeader('', home.headers);
  const html = await home.text();
  const csrf = CSRF_RE.exec(html)?.[1] || '';
  if (!csrf) throw new Error('1sec-mail: csrf token not found');
  return { csrf, cookie };
}

async function fetchMessages(session: OneSecMailSession): Promise<{ data: MessagesResponse; cookie: string }> {
  const response = await fetchWithTimeout(`${BASE_URL}get_messages`, {
    method: 'POST',
    headers: postHeaders(session),
    body: JSON.stringify({ _token: session.csrf, captcha: '' }),
  });
  if (!response.ok) throw new Error(`1sec-mail messages: ${response.status}`);
  const cookie = mergeCookieHeader(session.cookie, response.headers);
  const data = await response.json() as MessagesResponse;
  return { data, cookie };
}

function flattenMessage(raw: Record<string, unknown>, recipient: string): Record<string, unknown> {
  const content = typeof raw.content === 'string' ? raw.content : '';
  const html = raw.html === true ? content : (typeof raw.html === 'string' ? raw.html : '');
  return {
    ...raw,
    id: raw.id,
    from: raw.from_email || raw.from,
    to: raw.to || recipient,
    subject: raw.subject,
    text: raw.html === true ? '' : content,
    html,
    date: raw.receivedAt,
    isRead: raw.is_seen,
    attachments: raw.attachments,
  };
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const session = await openSession();
  const { data, cookie } = await fetchMessages(session);
  const email = String(data.mailbox || '').trim();
  if (!email || !email.includes('@')) {
    throw new Error('1sec-mail: invalid mailbox response');
  }
  return {
    channel: CHANNEL,
    email,
    token: encodeSession({ csrf: session.csrf, cookie }),
  };
}

export async function getEmails(token: string, email: string): Promise<Email[]> {
  const session = decodeSession(token);
  const address = String(email || '').trim();
  if (!address) throw new Error('1sec-mail: empty email');
  const { data } = await fetchMessages(session);
  const rows = Array.isArray(data.messages) ? data.messages : [];
  return rows.map((raw) => normalizeEmail(flattenMessage(raw, address), address));
}
