import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'tempmailo';
const BASE_URL = 'https://tempmailo.com';
const TOKEN_RE = /name="__RequestVerificationToken"[^>]*value="([^"]+)"/i;
const TOKEN_RE_REV = /value="([^"]+)"[^>]*name="__RequestVerificationToken"/i;

interface TempmailoSession {
  verificationToken: string;
  cookie: string;
}

function setCookieLines(headers: Headers): string[] {
  const h = headers as Headers & { getSetCookie?: () => string[] };
  if (typeof h.getSetCookie === 'function') {
    return h.getSetCookie();
  }
  const one = headers.get('set-cookie');
  return one ? [one] : [];
}

function mergeCookieHeader(prev: string, headers: Headers): string {
  const map = new Map<string, string>();
  for (const part of prev.split(';')) {
    const p = part.trim();
    const i = p.indexOf('=');
    if (i > 0) map.set(p.slice(0, i), p.slice(i + 1));
  }
  for (const line of setCookieLines(headers)) {
    const first = line.split(';')[0]?.trim() || '';
    const i = first.indexOf('=');
    if (i > 0) map.set(first.slice(0, i), first.slice(i + 1));
  }
  return [...map.entries()].map(([k, v]) => `${k}=${v}`).join('; ');
}

function pageHeaders(cookie?: string): Record<string, string> {
  return {
    Accept: 'text/html,application/json,*/*;q=0.8',
    Referer: `${BASE_URL}/`,
    'User-Agent':
      'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
    ...(cookie ? { Cookie: cookie } : {}),
  };
}

function parseVerificationToken(html: string): string {
  const token = TOKEN_RE.exec(html)?.[1] || TOKEN_RE_REV.exec(html)?.[1] || '';
  if (!token) throw new Error('tempmailo: verification token not found');
  return token;
}

function encodeSession(session: TempmailoSession): string {
  return JSON.stringify(session);
}

function decodeSession(token: string): TempmailoSession {
  const data = JSON.parse(token) as Partial<TempmailoSession>;
  const verificationToken = String(data.verificationToken || '').trim();
  const cookie = String(data.cookie || '').trim();
  if (!verificationToken || !cookie) {
    throw new Error('tempmailo: invalid session token');
  }
  return { verificationToken, cookie };
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const home = await fetchWithTimeout(BASE_URL, { headers: pageHeaders() });
  if (!home.ok) throw new Error(`tempmailo home: ${home.status}`);
  let cookie = mergeCookieHeader('', home.headers);
  const html = await home.text();
  const verificationToken = parseVerificationToken(html);

  const changed = await fetchWithTimeout(`${BASE_URL}/changemail?_r=${Math.random()}`, {
    headers: {
      ...pageHeaders(cookie),
      Accept: 'text/plain,*/*;q=0.8',
      RequestVerificationToken: verificationToken,
    },
  });
  if (!changed.ok) throw new Error(`tempmailo changemail: ${changed.status}`);
  cookie = mergeCookieHeader(cookie, changed.headers);
  const email = (await changed.text()).trim();
  if (!/^[^@\s]+@[^@\s]+$/.test(email)) {
    throw new Error('tempmailo: invalid email response');
  }

  return {
    channel: CHANNEL,
    email,
    token: encodeSession({ verificationToken, cookie }),
  };
}

export async function getEmails(token: string, email: string): Promise<Email[]> {
  const { verificationToken, cookie } = decodeSession(token);
  const address = (email || '').trim();
  if (!address) throw new Error('tempmailo: empty email');

  const response = await fetchWithTimeout(BASE_URL, {
    method: 'POST',
    headers: {
      ...pageHeaders(cookie),
      Accept: 'application/json,*/*;q=0.8',
      'Content-Type': 'application/json',
      RequestVerificationToken: verificationToken,
    },
    body: JSON.stringify({ mail: address }),
  });
  if (!response.ok) throw new Error(`tempmailo messages: ${response.status}`);
  const data = await response.json();
  const rows = Array.isArray(data) ? data : [];
  return rows.map((raw) => normalizeEmail(raw, address));
}
