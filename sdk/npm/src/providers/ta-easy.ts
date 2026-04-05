import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'ta-easy';
const API_BASE = 'https://api-endpoint.ta-easy.com';
const ORIGIN = 'https://www.ta-easy.com';

function browserHeaders(): Record<string, string> {
  return {
    Accept: 'application/json',
    'User-Agent':
      'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
    origin: ORIGIN,
    referer: `${ORIGIN}/`,
  };
}

/** POST /temp-email/address/new（空 body） */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const res = await fetchWithTimeout(`${API_BASE}/temp-email/address/new`, {
    method: 'POST',
    headers: { ...browserHeaders(), 'Content-Length': '0' },
  });
  if (!res.ok) {
    throw new Error(`ta-easy generate: ${res.status}`);
  }
  const data = await res.json();
  if (data.status !== 'success' || !data.address || !data.token) {
    throw new Error(data.message ? `ta-easy: ${data.message}` : 'ta-easy: create failed');
  }
  return {
    channel: CHANNEL,
    email: data.address as string,
    token: data.token as string,
    expiresAt: typeof data.expiresAt === 'number' ? data.expiresAt : undefined,
  };
}

/** POST /temp-email/inbox/list */
export async function getEmails(email: string, token: string): Promise<Email[]> {
  const res = await fetchWithTimeout(`${API_BASE}/temp-email/inbox/list`, {
    method: 'POST',
    headers: { ...browserHeaders(), 'Content-Type': 'application/json' },
    body: JSON.stringify({ token, email }),
  });
  if (!res.ok) {
    throw new Error(`ta-easy inbox: ${res.status}`);
  }
  const data = await res.json();
  if (data.status !== 'success') {
    throw new Error(data.message ? `ta-easy: ${data.message}` : 'ta-easy: inbox failed');
  }
  const list = Array.isArray(data.data) ? data.data : [];
  return list.map((raw: any) => normalizeEmail(raw, email));
}
