import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'throwawaymail';
const API_BASE = 'https://throwawaymail.app/api';

interface MailboxResponse {
  mailbox_id?: string;
  address?: string;
  expires_at?: string;
  created_at?: string;
}

interface MessageSummaryResponse {
  message_id?: string;
  from_address?: string;
  from_name?: string | null;
  subject?: string;
  received_at?: string;
  size?: number;
  read?: boolean;
}

interface MessageDetailResponse extends MessageSummaryResponse {
  to_addresses?: string[];
  cc_addresses?: string[] | null;
  bcc_addresses?: string[] | null;
  reply_to?: string | null;
  text?: string | null;
  html?: string | null;
}

function normalizeRaw(raw: MessageSummaryResponse | MessageDetailResponse, recipient: string): Record<string, unknown> {
  return {
    id: raw.message_id,
    messageId: raw.message_id,
    from_address: raw.from_address,
    fromName: raw.from_name,
    to: Array.isArray((raw as MessageDetailResponse).to_addresses) && (raw as MessageDetailResponse).to_addresses!.length > 0
      ? (raw as MessageDetailResponse).to_addresses![0]
      : recipient,
    subject: raw.subject || '',
    received_at: raw.received_at,
    read: raw.read,
    text: (raw as MessageDetailResponse).text || '',
    html: (raw as MessageDetailResponse).html || '',
    size: raw.size,
  };
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
    throw new Error(`throwawaymail http ${response.status}`);
  }
  return response.json() as Promise<T>;
}

async function fetchDetail(mailboxId: string, messageId: string): Promise<MessageDetailResponse | null> {
  const url = `${API_BASE}/mailboxes/${encodeURIComponent(mailboxId)}/messages/${encodeURIComponent(messageId)}`;
  try {
    return await fetchJSON<MessageDetailResponse>(url);
  } catch {
    return null;
  }
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const data = await fetchJSON<MailboxResponse>(`${API_BASE}/mailboxes`, { method: 'POST' });
  const mailboxId = String(data.mailbox_id || '').trim();
  const email = String(data.address || '').trim();
  if (!mailboxId || !email || !email.includes('@')) {
    throw new Error('throwawaymail: invalid mailbox response');
  }

  return {
    channel: CHANNEL,
    email,
    token: mailboxId,
    expiresAt: data.expires_at,
    createdAt: data.created_at,
  };
}

export async function getEmails(token: string, email: string): Promise<Email[]> {
  const mailboxId = String(token || '').trim();
  const address = String(email || '').trim();
  if (!mailboxId) throw new Error('throwawaymail: empty mailbox id');
  if (!address) throw new Error('throwawaymail: empty email');

  const url = `${API_BASE}/mailboxes/${encodeURIComponent(mailboxId)}/messages`;
  const rows = await fetchJSON<MessageSummaryResponse[]>(url);
  if (!Array.isArray(rows)) return [];

  const emails: Email[] = [];
  for (const row of rows) {
    const messageId = String(row.message_id || '').trim();
    const detail = messageId ? await fetchDetail(mailboxId, messageId) : null;
    emails.push(normalizeEmail(normalizeRaw(detail || row, address), address));
  }
  return emails;
}
