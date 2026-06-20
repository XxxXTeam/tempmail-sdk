import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'catchmail';
const API_BASE = 'https://api.catchmail.io/api/v1';
const DEFAULT_DOMAIN = 'catchmail.io';
const DOMAINS = ['catchmail.io', 'mailistry.com', 'zeppost.com'];

const HEADERS: Record<string, string> = {
  Accept: 'application/json',
  Referer: 'https://catchmail.io/',
  Origin: 'https://catchmail.io',
  'User-Agent':
    'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
};

interface MailboxResponse {
  messages?: CatchmailListMessage[];
}

interface CatchmailListMessage {
  id?: string;
  mailbox?: string;
  from?: string;
  subject?: string;
  date?: string;
  size?: number;
}

interface CatchmailDetail {
  id?: string;
  mailbox?: string;
  from?: string;
  to?: string[] | string;
  subject?: string;
  date?: string;
  body?: {
    text?: string;
    html?: string;
  };
  attachments?: unknown[];
}

function randomLocal(): string {
  const chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
  let out = 'sdk';
  for (let i = 0; i < 16; i++) out += chars[Math.floor(Math.random() * chars.length)];
  return out;
}

function pickDomain(preferred?: string | null): string {
  const p = (preferred || '').trim().replace(/^@/, '').toLowerCase();
  if (p) {
    const hit = DOMAINS.find((d) => d.toLowerCase() === p);
    if (hit) return hit;
  }
  return DEFAULT_DOMAIN;
}

function cleanAddress(value: unknown): string {
  if (Array.isArray(value)) return cleanAddress(value[0]);
  const text = String(value ?? '').trim();
  const angle = text.match(/<([^>]+)>/);
  return angle ? angle[1].trim() : text;
}

function flattenDetail(raw: CatchmailDetail, recipient: string): Record<string, unknown> {
  return {
    id: raw.id || '',
    from: cleanAddress(raw.from),
    to: cleanAddress(raw.to) || recipient,
    subject: raw.subject || '',
    text: raw.body?.text || '',
    html: raw.body?.html || '',
    date: raw.date || '',
    attachments: raw.attachments,
  };
}

async function fetchMessage(id: string, email: string): Promise<CatchmailDetail | null> {
  const url = `${API_BASE}/message/${encodeURIComponent(id)}?mailbox=${encodeURIComponent(email)}`;
  const response = await fetchWithTimeout(url, { method: 'GET', headers: HEADERS });
  if (!response.ok) return null;
  const data = (await response.json()) as CatchmailDetail;
  return data && typeof data === 'object' ? data : null;
}

export async function generateEmail(domain?: string | null, channel: Channel = CHANNEL): Promise<InternalEmailInfo> {
  const selectedDomain = pickDomain(domain);
  const email = `${randomLocal()}@${selectedDomain}`;

  const response = await fetchWithTimeout(
    `${API_BASE}/mailbox?address=${encodeURIComponent(email)}`,
    { method: 'GET', headers: HEADERS },
  );
  if (!response.ok) {
    throw new Error(`catchmail: create mailbox failed HTTP ${response.status}`);
  }

  return { channel, email };
}

export async function getEmails(email: string): Promise<Email[]> {
  const address = (email || '').trim();
  if (!address) throw new Error('catchmail: empty email');

  const response = await fetchWithTimeout(
    `${API_BASE}/mailbox?address=${encodeURIComponent(address)}`,
    { method: 'GET', headers: HEADERS },
  );
  if (!response.ok) {
    throw new Error(`catchmail: list mailbox failed HTTP ${response.status}`);
  }

  const data = (await response.json()) as MailboxResponse;
  const list = Array.isArray(data.messages) ? data.messages : [];
  const emails: Email[] = [];
  for (const item of list) {
    if (!item.id) continue;
    const detail = await fetchMessage(String(item.id), address);
    if (detail) {
      emails.push(normalizeEmail(flattenDetail(detail, address), address));
      continue;
    }
    emails.push(normalizeEmail({
      id: item.id,
      from: cleanAddress(item.from),
      to: item.mailbox || address,
      subject: item.subject || '',
      date: item.date || '',
    }, address));
  }
  return emails;
}
