import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'fmail';
const BASE_URL = 'https://fmail.men';

interface RandomResponse {
  username?: string;
  domain?: string;
  address?: string;
}

interface InboxRow {
  id?: string | number;
  token?: string;
  sender?: string;
  subject?: string;
  snippet?: string;
  received_at?: string | number;
}

interface InboxResponse {
  emails?: InboxRow[];
}

interface EmailDetailResponse {
  token?: string;
  recipient?: string;
  domain?: string;
  sender?: string;
  subject?: string;
  body_text?: string;
  body_html?: string;
  received_at?: string | number;
}

async function fetchJSON<T>(url: string, init?: RequestInit): Promise<T> {
  const response = await fetchWithTimeout(url, {
    ...init,
    headers: {
      Accept: 'application/json',
      ...(init?.headers || {}),
    },
  });
  if (!response.ok) {
    throw new Error(`fmail http ${response.status}`);
  }
  return response.json() as Promise<T>;
}

function flattenMessage(raw: Record<string, unknown>, recipient: string): Record<string, unknown> {
  return {
    id: raw.id || raw.uid || raw.token || '',
    from: raw.sender || raw.from || '',
    to: raw.recipient || raw.to || recipient,
    subject: raw.subject || '',
    text: raw.body_text || raw.text || raw.snippet || '',
    html: raw.body_html || raw.html || '',
    date: raw.received_at || raw.timestamp || raw.date || '',
    attachments: raw.attachments || [],
  };
}

function normalizeDomain(domain?: string | null): string | undefined {
  const value = String(domain || '').trim();
  if (!value) {
    return undefined;
  }
  return value.replace(/^@/, '');
}

export async function generateEmail(domain?: string | null): Promise<InternalEmailInfo> {
  const selectedDomain = normalizeDomain(domain);
  const url = selectedDomain
    ? `${BASE_URL}/v1/random?domain=${encodeURIComponent(selectedDomain)}`
    : `${BASE_URL}/v1/random`;
  const json = await fetchJSON<RandomResponse>(url);
  const email = String(json?.address || '').trim();
  if (!email || !email.includes('@')) {
    throw new Error('fmail: invalid random response');
  }
  return { channel: CHANNEL, email };
}

export async function getEmails(email: string): Promise<Email[]> {
  const [local, domain] = String(email || '').split('@');
  if (!local || !domain) {
    throw new Error('fmail: invalid email');
  }

  const json = await fetchJSON<InboxResponse>(
    `${BASE_URL}/v1/inbox/${encodeURIComponent(local)}?domain=${encodeURIComponent(domain)}&limit=50`,
  );
  const rows = Array.isArray(json?.emails) ? json.emails : [];
  const out: Email[] = [];

  for (const row of rows) {
    const token = String(row?.token || row?.id || '').trim();
    if (!token) {
      out.push(normalizeEmail(flattenMessage(row as Record<string, unknown>, email), email));
      continue;
    }

    try {
      const detail = await fetchJSON<EmailDetailResponse>(`${BASE_URL}/v1/email/${encodeURIComponent(token)}`);
      out.push(normalizeEmail(flattenMessage(detail as Record<string, unknown>, email), email));
    } catch {
      out.push(normalizeEmail(flattenMessage(row as Record<string, unknown>, email), email));
    }
  }

  return out;
}
