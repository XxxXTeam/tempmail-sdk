import { InternalEmailInfo, Email } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL = 'emailnator';
const BASE_URL = 'https://www.emailnator.com';
const EMAIL_OPTIONS = ['plusGmail', 'dotGmail', 'googleMail'];

const HEADERS: Record<string, string> = {
  'Accept': 'application/json, text/plain, */*',
  'Content-Type': 'application/json',
  'Origin': BASE_URL,
  'Referer': `${BASE_URL}/`,
  'User-Agent':
    'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36',
  'X-Requested-With': 'XMLHttpRequest',
};

type EmailnatorSession = {
  xsrfToken: string;
  cookie: string;
};

type GenerateResponse = {
  email?: string[];
};

type MessageRow = {
  messageID?: string;
  from?: string;
  subject?: string;
  time?: string;
};

type ListResponse = {
  messageData?: MessageRow[];
};

function cookieHeader(headers: Headers): string {
  const getSetCookie = (headers as any).getSetCookie?.bind(headers);
  const values: string[] = typeof getSetCookie === 'function'
    ? getSetCookie()
    : (headers.get('set-cookie') ? [headers.get('set-cookie') as string] : []);
  return values.map((value) => value.split(';')[0]).filter(Boolean).join('; ');
}

function xsrfToken(headers: Headers): string {
  const cookie = cookieHeader(headers);
  const part = cookie.split(';').map((item) => item.trim()).find((item) => item.startsWith('XSRF-TOKEN='));
  const raw = part ? part.slice('XSRF-TOKEN='.length) : '';
  return raw ? decodeURIComponent(raw) : '';
}

async function initSession(): Promise<EmailnatorSession> {
  const response = await fetchWithTimeout(BASE_URL, {
    headers: {
      Accept: 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
      'User-Agent': HEADERS['User-Agent'],
    },
  });
  if (!response.ok) throw new Error(`emailnator home: ${response.status}`);
  const cookie = cookieHeader(response.headers);
  const xsrf = xsrfToken(response.headers);
  if (!cookie || !xsrf) throw new Error('emailnator: missing session cookie or XSRF token');
  return { xsrfToken: xsrf, cookie };
}

async function postJSON<T>(session: EmailnatorSession, path: string, body: unknown): Promise<T> {
  const text = await postText(session, path, body);
  return JSON.parse(text) as T;
}

async function postText(session: EmailnatorSession, path: string, body: unknown): Promise<string> {
  const response = await fetchWithTimeout(`${BASE_URL}${path}`, {
    method: 'POST',
    headers: {
      ...HEADERS,
      'Cookie': session.cookie,
      'X-XSRF-TOKEN': session.xsrfToken,
    },
    body: JSON.stringify(body),
  });
  const text = await response.text();
  if (!response.ok) {
    throw new Error(`emailnator ${path}: ${response.status} ${text.slice(0, 160)}`);
  }
  return text;
}

function encodeSession(session: EmailnatorSession): string {
  return JSON.stringify(session);
}

function decodeSession(token: string): EmailnatorSession {
  const data = JSON.parse(token) as Partial<EmailnatorSession>;
  if (!data.cookie || !data.xsrfToken) throw new Error('emailnator: invalid session token');
  return { cookie: data.cookie, xsrfToken: data.xsrfToken };
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const session = await initSession();
  const data = await postJSON<GenerateResponse>(session, '/generate-email', { email: EMAIL_OPTIONS });
  const email = data.email?.find((item) => typeof item === 'string' && item.includes('@')) || '';
  if (!email) throw new Error('emailnator: empty email response');
  return {
    channel: CHANNEL,
    email,
    token: encodeSession(session),
  };
}

async function fetchDetail(session: EmailnatorSession, email: string, messageID: string): Promise<string> {
  return postText(session, '/message-list', { email, messageID });
}

export async function getEmails(token: string, email: string): Promise<Email[]> {
  const session = decodeSession(token);
  const data = await postJSON<ListResponse>(session, '/message-list', { email });
  const rows = Array.isArray(data.messageData) ? data.messageData : [];
  const out: Email[] = [];

  for (const row of rows) {
    const messageID = String(row.messageID || '');
    if (!messageID || messageID.startsWith('ADS')) continue;
    let html = '';
    try {
      html = await fetchDetail(session, email, messageID);
    } catch {
      html = '';
    }
    out.push(normalizeEmail({
      id: messageID,
      from: row.from || '',
      to: email,
      subject: row.subject || '',
      html,
      date: row.time || '',
      isRead: false,
      attachments: [],
    }, email));
  }

  return out;
}
