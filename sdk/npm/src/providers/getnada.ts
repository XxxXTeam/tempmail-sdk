import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'getnada';
const API_BASE = 'https://getnada.net/api';
const DOMAIN_LABEL_RE = /^[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$/i;

interface DomainsResponse {
  domains?: string[];
}

interface OpenResponse {
  inboxId?: string;
  token?: string;
  activeUntil?: string;
  recipient?: string;
}

interface MessagesResponse {
  messages?: Array<Record<string, unknown>>;
}

interface MessageDetailResponse {
  message?: Record<string, unknown>;
}

function randomLocal(): string {
  const chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
  let out = 'sdk';
  for (let i = 0; i < 16; i++) out += chars[Math.floor(Math.random() * chars.length)];
  return out;
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
    throw new Error(`getnada http ${response.status}`);
  }
  return response.json() as Promise<T>;
}

function flattenMessage(raw: Record<string, unknown>, recipient: string): Record<string, unknown> {
  return {
    ...raw,
    id: raw.id,
    from: raw.from_addr || raw.from,
    to: raw.to || recipient,
    text: raw.text_plain || raw.text,
    html: raw.html_sanitized || raw.html,
    read: raw.read_at ? true : raw.read,
  };
}

async function pickDomain(preferred?: string | null): Promise<string> {
  const data = await fetchJSON<DomainsResponse>(`${API_BASE}/public/domains`);
  const domains = (Array.isArray(data.domains) ? data.domains : [])
    .map((domain) => String(domain || '').trim().toLowerCase())
    .filter(isMailDomain);
  const wanted = String(preferred || '').trim().replace(/^@/, '').toLowerCase();
  if (wanted) {
    const exact = domains.find((domain) => domain === wanted);
    if (!exact) throw new Error(`getnada: domain not available: ${wanted}`);
    return exact;
  }
  return domains.find((domain) => domain === 'getnada.net') ?? domains[0] ?? '';
}

function isMailDomain(domain: string): boolean {
  if (!domain || domain.length > 253 || domain.includes('..')) return false;
  const labels = domain.split('.');
  return labels.length >= 2 && labels.every((label) => DOMAIN_LABEL_RE.test(label));
}

export async function generateEmail(domain?: string | null, channel: Channel = CHANNEL): Promise<InternalEmailInfo> {
  const selectedDomain = await pickDomain(domain);
  if (!selectedDomain) throw new Error('getnada: no domain available');
  const requested = `${randomLocal()}@${selectedDomain}`;
  const data = await fetchJSON<OpenResponse>(`${API_BASE}/inbox/open`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ email: requested }),
  });

  const token = String(data.token || '').trim();
  const email = String(data.recipient || requested).trim();
  if (!token || !email || !email.includes('@')) {
    throw new Error('getnada: invalid open response');
  }
  return {
    channel,
    email,
    token,
    expiresAt: data.activeUntil,
    createdAt: undefined,
  };
}

async function fetchDetail(token: string, messageId: string): Promise<Record<string, unknown> | null> {
  const url = `${API_BASE}/inbox/message?id=${encodeURIComponent(messageId)}&token=${encodeURIComponent(token)}`;
  try {
    const data = await fetchJSON<MessageDetailResponse>(url);
    return data.message && typeof data.message === 'object' ? data.message : null;
  } catch {
    return null;
  }
}

export async function getEmails(token: string, email: string): Promise<Email[]> {
  const auth = String(token || '').trim();
  if (!auth) throw new Error('getnada: empty token');
  const address = String(email || '').trim();
  if (!address) throw new Error('getnada: empty email');

  const data = await fetchJSON<MessagesResponse>(`${API_BASE}/inbox/messages?token=${encodeURIComponent(auth)}`);
  const rows = Array.isArray(data.messages) ? data.messages : [];

  const emails: Email[] = [];
  for (const row of rows) {
    const id = row.id == null ? '' : String(row.id);
    const detail = id ? await fetchDetail(auth, id) : null;
    emails.push(normalizeEmail(flattenMessage(detail || row, address), address));
  }
  return emails;
}
