import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'shitty-email';
const API_BASE = 'https://shitty.email/api';

interface InboxResponse {
  email?: string;
  token?: string;
  expiresAt?: string;
  emails?: Array<Record<string, unknown>>;
}

function flattenMessage(raw: Record<string, unknown>, recipient: string): Record<string, unknown> {
  return {
    id: raw.id,
    from: raw.from,
    to: raw.to || recipient,
    subject: raw.subject,
    text: raw.text,
    html: raw.html,
    date: raw.date,
  };
}

async function fetchJSON<T>(url: string, token?: string, init?: RequestInit): Promise<T> {
  const response = await fetchWithTimeout(url, {
    ...init,
    headers: {
      Accept: 'application/json',
      'User-Agent': 'Mozilla/5.0',
      ...(token ? { 'X-Session-Token': token } : {}),
      ...(init?.headers || {}),
    },
  });
  if (!response.ok) {
    throw new Error(`shitty-email http ${response.status}`);
  }
  return response.json() as Promise<T>;
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const data = await fetchJSON<InboxResponse>(`${API_BASE}/inbox`, undefined, { method: 'POST' });
  const email = String(data.email || '').trim();
  const token = String(data.token || '').trim();
  if (!email || !email.includes('@') || !token) {
    throw new Error('shitty-email: invalid inbox response');
  }
  return {
    channel: CHANNEL,
    email,
    token,
    expiresAt: data.expiresAt,
  };
}

export async function getEmails(token: string, email: string): Promise<Email[]> {
  const sessionToken = String(token || '').trim();
  const address = String(email || '').trim();
  if (!sessionToken) throw new Error('shitty-email: empty token');
  if (!address) throw new Error('shitty-email: empty email');

  const data = await fetchJSON<InboxResponse>(`${API_BASE}/inbox`, sessionToken);
  const rows = Array.isArray(data.emails) ? data.emails : [];
  const emails: Email[] = [];
  for (const row of rows) {
    const id = String(row.id || '').trim();
    let raw = row;
    if (id) {
      try {
        raw = await fetchJSON<Record<string, unknown>>(`${API_BASE}/email/${encodeURIComponent(id)}`, sessionToken);
      } catch {}
    }
    emails.push(normalizeEmail(flattenMessage(raw, address), address));
  }
  return emails;
}
