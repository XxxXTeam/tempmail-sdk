import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'mffac';
const BASE_URL = 'https://www.mffac.com/api';

const DEFAULT_HEADERS = {
  'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
  'Content-Type': 'application/json',
  'Accept': '*/*',
  'Origin': 'https://www.mffac.com',
  'Referer': 'https://www.mffac.com/',
  'sec-ch-ua': '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
  'sec-ch-ua-mobile': '?0',
  'sec-ch-ua-platform': '"Windows"',
  'sec-fetch-dest': 'empty',
  'sec-fetch-mode': 'cors',
  'sec-fetch-site': 'same-origin',
};

const GET_HEADERS = Object.fromEntries(
  Object.entries(DEFAULT_HEADERS).filter(([key]) => key.toLowerCase() !== 'content-type'),
) as Record<string, string>;

function receivedAtToIso(value: unknown): string {
  const seconds = typeof value === 'number' ? value : Number(value);
  if (!Number.isFinite(seconds) || seconds <= 0) return '';
  return new Date(seconds * 1000).toISOString();
}

function flattenEmail(raw: any, recipient: string): Record<string, unknown> {
  return {
    id: raw.id,
    from: raw.fromAddress,
    to: raw.toAddress || recipient,
    subject: raw.subject || '',
    text: raw.textContent || '',
    html: raw.htmlContent || '',
    date: receivedAtToIso(raw.receivedAt),
    isRead: raw.isRead,
    attachments: [],
  };
}

async function fetchEmailDetail(id: string): Promise<any | null> {
  const response = await fetchWithTimeout(`${BASE_URL}/emails/${encodeURIComponent(id)}`, {
    method: 'GET',
    headers: GET_HEADERS,
  });
  if (!response.ok) return null;
  const data = await response.json();
  return data && data.success && data.email && typeof data.email === 'object' ? data.email : null;
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const response = await fetchWithTimeout(`${BASE_URL}/mailboxes`, {
    method: 'POST',
    headers: DEFAULT_HEADERS,
    body: JSON.stringify({ expiresInHours: 24 }),
  });

  if (!response.ok) {
    throw new Error(`Failed to generate email: ${response.status}`);
  }

  const data = await response.json();

  if (!data.success || !data.mailbox) {
    throw new Error('Failed to generate email');
  }

  const mailbox = data.mailbox;
  const email = `${mailbox.address}@mffac.com`;

  return {
    channel: CHANNEL,
    email,
    expiresAt: mailbox.expiresAt,
    createdAt: mailbox.createdAt,
    token: mailbox.id,
  };
}

export async function getEmails(email: string, _token?: string): Promise<Email[]> {
  const address = email.split('@')[0];
  const response = await fetchWithTimeout(`${BASE_URL}/mailboxes/${address}/emails`, {
    method: 'GET',
    headers: GET_HEADERS,
  });

  if (!response.ok) {
    throw new Error(`Failed to get emails: ${response.status}`);
  }

  const data = await response.json();

  if (!data.success) {
    throw new Error('Failed to get emails');
  }

  const rawEmails = data.emails || [];
  const emails: Email[] = [];
  for (const raw of rawEmails) {
    const id = String(raw?.id || '').trim();
    const detail = id ? await fetchEmailDetail(id) : null;
    emails.push(normalizeEmail(flattenEmail(detail || raw, email), email));
  }
  return emails;
}
