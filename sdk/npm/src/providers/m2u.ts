import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'm2u';
const API_BASE = 'https://api.m2u.io';

type Mailbox = {
  token?: string;
  view_token?: string;
  local_part?: string;
  domain?: string;
  expires_at?: string | number;
  created_at?: string | number;
};

type CreateResponse = {
  mailbox?: Mailbox;
};

type ListResponse = {
  messages?: Array<Record<string, unknown>>;
};

type DetailResponse = {
  message?: Record<string, unknown>;
};

function headers(extra?: Record<string, string>): Record<string, string> {
  return {
    Accept: 'application/json',
    'User-Agent':
      'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36',
    ...(extra || {}),
  };
}

async function fetchJson<T>(url: string, init?: RequestInit): Promise<T> {
  const response = await fetchWithTimeout(url, {
    ...init,
    headers: {
      ...headers(),
      ...(init?.headers || {}),
    },
  });
  if (!response.ok) {
    throw new Error(`m2u http ${response.status}`);
  }
  return response.json() as Promise<T>;
}

function packToken(token: string, viewToken: string): string {
  return JSON.stringify({ token, viewToken });
}

function unpackToken(packed: string): { token: string; viewToken: string } {
  const value = String(packed || '').trim();
  if (!value) {
    return { token: '', viewToken: '' };
  }
  if (value.startsWith('{')) {
    try {
      const data = JSON.parse(value) as Record<string, unknown>;
      return {
        token: typeof data.token === 'string' ? data.token.trim() : '',
        viewToken: typeof data.viewToken === 'string' ? data.viewToken.trim() : '',
      };
    } catch {
      return { token: '', viewToken: '' };
    }
  }
  return { token: value, viewToken: '' };
}

function flattenMessage(raw: Record<string, unknown>, recipient: string): Record<string, unknown> {
  return {
    id: raw.id || raw.message_id,
    from: raw.from_addr || raw.from_address || raw.from || '',
    to: recipient,
    subject: raw.subject || '',
    text: raw.text_body || raw.body_text || raw.text || '',
    html: raw.html_body || raw.body_html || raw.html || '',
    date: raw.received_at || raw.created_at || '',
    isRead: raw.is_read ?? raw.isRead ?? raw.seen ?? false,
    attachments: raw.attachments || [],
  };
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const data = await fetchJson<CreateResponse>(`${API_BASE}/v1/mailboxes/auto`, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({}),
  });
  const mailbox = data.mailbox || {};
  const localPart = String(mailbox.local_part || '').trim();
  const domain = String(mailbox.domain || '').trim();
  const token = String(mailbox.token || '').trim();
  const viewToken = String(mailbox.view_token || '').trim();
  const email = localPart && domain ? `${localPart}@${domain}` : '';
  if (!email || !token || !viewToken) {
    throw new Error('m2u: invalid mailbox response');
  }
  return {
    channel: CHANNEL,
    email,
    token: packToken(token, viewToken),
    expiresAt: mailbox.expires_at,
    createdAt: mailbox.created_at == null ? undefined : String(mailbox.created_at),
  };
}

async function fetchDetail(token: string, viewToken: string, messageId: string): Promise<Record<string, unknown> | null> {
  try {
    const data = await fetchJson<DetailResponse>(
      `${API_BASE}/v1/mailboxes/${encodeURIComponent(token)}/messages/${encodeURIComponent(messageId)}?view=${encodeURIComponent(viewToken)}`,
    );
    return data.message && typeof data.message === 'object' ? data.message : null;
  } catch {
    return null;
  }
}

export async function getEmails(token: string, email: string): Promise<Email[]> {
  const { token: mailboxToken, viewToken } = unpackToken(token);
  const address = String(email || '').trim();
  if (!mailboxToken) throw new Error('m2u: missing token');
  if (!viewToken) throw new Error('m2u: missing view token');
  if (!address) throw new Error('m2u: empty email');

  const data = await fetchJson<ListResponse>(
    `${API_BASE}/v1/mailboxes/${encodeURIComponent(mailboxToken)}/messages?view=${encodeURIComponent(viewToken)}`,
  );
  const rows = Array.isArray(data.messages) ? data.messages : [];
  const emails: Email[] = [];
  for (const row of rows) {
    const id = String(row.id || row.message_id || '').trim();
    const detail = id ? await fetchDetail(mailboxToken, viewToken, id) : null;
    emails.push(normalizeEmail(flattenMessage(detail || row, address), address));
  }
  return emails;
}
