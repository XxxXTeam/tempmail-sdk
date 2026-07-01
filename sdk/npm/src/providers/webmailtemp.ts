import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'webmailtemp';
const BASE_URL = 'https://webmailtemp.com';

interface CreateResponse {
  success?: boolean;
  email?: string;
  username?: string;
  created_at?: number;
  ttl?: number;
}

interface CheckResponse {
  success?: boolean;
  emails?: Array<Record<string, unknown>>;
}

interface TokenData {
  u: string;
  c: string;
}

function encodeToken(data: TokenData): string {
  return `wmt1:${Buffer.from(JSON.stringify(data), 'utf8').toString('base64')}`;
}

function decodeToken(token: string): TokenData {
  if (!token.startsWith('wmt1:')) throw new Error('webmailtemp: invalid token');
  const raw = Buffer.from(token.slice(5), 'base64').toString('utf8');
  const data = JSON.parse(raw) as Partial<TokenData>;
  if (!data.u || !data.c) throw new Error('webmailtemp: invalid token data');
  return { u: data.u, c: data.c };
}

function flattenMessage(raw: Record<string, unknown>, recipient: string): Record<string, unknown> {
  return {
    id: raw.id,
    from: raw.from,
    to: recipient,
    subject: raw.subject,
    text: raw.body,
    html: raw.html,
    date: raw.received_at || raw.timestamp,
    attachments: raw.attachments,
  };
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const response = await fetchWithTimeout(`${BASE_URL}/api/create`, {
    headers: {
      Accept: 'application/json',
      'User-Agent': 'Mozilla/5.0',
    },
  });
  if (!response.ok) {
    throw new Error(`webmailtemp create http ${response.status}`);
  }
  const cookie = response.headers.get('set-cookie')?.split(';')[0] || '';
  const data = (await response.json()) as CreateResponse;
  const email = String(data.email || '').trim();
  const username = String(data.username || '').trim();
  if (!data.success || !email || !email.includes('@') || !username || !cookie) {
    throw new Error('webmailtemp: invalid create response');
  }
  return {
    channel: CHANNEL,
    email,
    token: encodeToken({ u: username, c: cookie }),
    expiresAt: typeof data.ttl === 'number' ? Date.now() + data.ttl * 1000 : undefined,
    createdAt: typeof data.created_at === 'number' ? new Date(data.created_at * 1000).toISOString() : undefined,
  };
}

export async function getEmails(token: string, email: string): Promise<Email[]> {
  const address = String(email || '').trim();
  if (!address) throw new Error('webmailtemp: empty email');
  const session = decodeToken(token);

  const response = await fetchWithTimeout(`${BASE_URL}/api/check/${encodeURIComponent(session.u)}`, {
    headers: {
      Accept: 'application/json',
      Cookie: session.c,
      'User-Agent': 'Mozilla/5.0',
    },
  });
  if (!response.ok) {
    throw new Error(`webmailtemp check http ${response.status}`);
  }
  const data = (await response.json()) as CheckResponse;
  const messages = Array.isArray(data.emails) ? data.emails : [];
  return messages.map((row) => normalizeEmail(flattenMessage(row, address), address));
}
