import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'tempmail-lol-v2';
const API_BASE = 'https://api.tempmail.lol';

const HEADERS: Record<string, string> = {
  'Accept': 'application/json',
  'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36',
};

export async function generateEmail(): Promise<InternalEmailInfo> {
  const res = await fetchWithTimeout(`${API_BASE}/generate`, { headers: HEADERS });
  if (!res.ok) throw new Error(`tempmail-lol-v2: generate failed: ${res.status}`);
  const data = await res.json() as { address?: string; token?: string };
  if (!data.address || !data.token) throw new Error('tempmail-lol-v2: missing address or token');
  return { channel: CHANNEL, email: data.address, token: data.token };
}

export async function getEmails(token: string, email: string): Promise<Email[]> {
  const res = await fetchWithTimeout(`${API_BASE}/auth/${encodeURIComponent(token)}`, { headers: HEADERS });
  if (!res.ok) throw new Error(`tempmail-lol-v2: get emails failed: ${res.status}`);
  const data = await res.json() as { email?: any[] };
  const list = Array.isArray(data.email) ? data.email : [];
  return list.map((raw: any) => normalizeEmail({
    id: raw.id || raw._id || '',
    from: raw.from || raw.sender || '',
    to: email,
    subject: raw.subject || '',
    text: raw.body || raw.text || '',
    html: raw.html || raw.body || '',
    date: raw.date || raw.receivedAt || '',
    isRead: false,
  }, email));
}
