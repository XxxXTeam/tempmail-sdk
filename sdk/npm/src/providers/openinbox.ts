import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'openinbox';
const API_BASE = 'https://api.openinbox.io/api';

interface CreateResponse {
  id?: string;
  email?: string;
  expiresAt?: string;
  createdAt?: string;
}

interface ListResponse {
  emails?: ListMessage[];
}

interface ListMessage {
  id?: string;
  from?: string;
  subject?: string;
  receivedAt?: string;
  isRead?: boolean;
  hasAttachments?: boolean;
  threadId?: string;
}

interface DetailResponse extends ListMessage {
  textBody?: string;
  htmlBody?: string;
  headers?: unknown;
}

const HEADERS: Record<string, string> = {
  Accept: 'application/json, text/plain, */*',
  Origin: 'https://openinbox.io',
  Referer: 'https://openinbox.io/',
  'User-Agent': 'Mozilla/5.0',
};

async function fetchJSON<T>(url: string, init?: RequestInit): Promise<T> {
  const response = await fetchWithTimeout(url, {
    ...init,
    headers: {
      ...HEADERS,
      ...(init?.headers || {}),
    },
  });
  if (!response.ok) {
    throw new Error(`openinbox http ${response.status}`);
  }
  return response.json() as Promise<T>;
}

function flatten(raw: DetailResponse | ListMessage, recipient: string): Record<string, unknown> {
  return {
    id: raw.id || '',
    from: raw.from || '',
    to: recipient,
    subject: raw.subject || '',
    text: (raw as DetailResponse).textBody || '',
    html: (raw as DetailResponse).htmlBody || '',
    receivedAt: raw.receivedAt || '',
    isRead: Boolean(raw.isRead),
    attachments: [],
  };
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const data = await fetchJSON<CreateResponse>(`${API_BASE}/inbox`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
  });
  const id = String(data.id || '').trim();
  const email = String(data.email || '').trim();
  if (!id || !email || !email.includes('@')) {
    throw new Error('openinbox: invalid mailbox response');
  }
  return {
    channel: CHANNEL,
    email,
    token: id,
    expiresAt: data.expiresAt,
    createdAt: data.createdAt,
  };
}

async function fetchDetail(id: string): Promise<DetailResponse | null> {
  try {
    return await fetchJSON<DetailResponse>(`${API_BASE}/emails/${encodeURIComponent(id)}`);
  } catch {
    return null;
  }
}

export async function getEmails(token: string, email: string): Promise<Email[]> {
  const inboxId = String(token || '').trim();
  const address = String(email || '').trim();
  if (!inboxId) throw new Error('openinbox: empty inbox id');
  if (!address) throw new Error('openinbox: empty email');

  const data = await fetchJSON<ListResponse>(
    `${API_BASE}/emails/inbox/${encodeURIComponent(inboxId)}?page=1&limit=50`,
  );
  const rows = Array.isArray(data.emails) ? data.emails : [];
  const emails: Email[] = [];
  for (const row of rows) {
    const id = String(row.id || '').trim();
    const detail = id ? await fetchDetail(id) : null;
    emails.push(normalizeEmail(flatten(detail || row, address), address));
  }
  return emails;
}
