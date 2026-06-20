import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'inboxkitten';
const API_BASE = 'https://inboxkitten.com/api/v1/mail';
const DOMAIN = 'inboxkitten.com';

interface ListItem {
  storage?: {
    region?: string;
    key?: string;
  };
  message?: {
    headers?: {
      from?: string;
      subject?: string;
    };
  };
  envelope?: {
    sender?: string;
    targets?: string;
  };
  recipient?: string;
  timestamp?: number;
}

interface MessageMeta {
  name?: string;
  subject?: string;
  recipients?: string;
}

function randomLocal(): string {
  const chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
  let out = 'sdk';
  for (let i = 0; i < 16; i++) out += chars[Math.floor(Math.random() * chars.length)];
  return out;
}

function decodeHtmlEntities(s: string): string {
  return s
    .replace(/&#x([0-9a-fA-F]+);/g, (_, h) => String.fromCodePoint(parseInt(h, 16)))
    .replace(/&#(\d+);/g, (_, d) => String.fromCodePoint(parseInt(d, 10)))
    .replace(/&nbsp;/g, ' ')
    .replace(/&amp;/g, '&')
    .replace(/&lt;/g, '<')
    .replace(/&gt;/g, '>')
    .replace(/&quot;/g, '"')
    .replace(/&#39;/g, "'");
}

function htmlToText(html: string): string {
  return decodeHtmlEntities(html.replace(/<script[\s\S]*?<\/script>/gi, ' ').replace(/<[^>]+>/g, ' '))
    .replace(/\s+/g, ' ')
    .trim();
}

function localPart(email: string): string {
  return String(email || '').trim().split('@')[0] || '';
}

async function fetchJSON<T>(url: string): Promise<T> {
  const response = await fetchWithTimeout(url, {
    method: 'GET',
    headers: { Accept: 'application/json' },
  });
  if (!response.ok) {
    throw new Error(`inboxkitten http ${response.status}`);
  }
  return response.json() as Promise<T>;
}

async function fetchHtml(region: string, key: string): Promise<string> {
  const url = `${API_BASE}/getHtml?region=${encodeURIComponent(region)}&key=${encodeURIComponent(key)}`;
  const response = await fetchWithTimeout(url, {
    method: 'GET',
    headers: { Accept: 'text/html,*/*' },
  });
  if (!response.ok) {
    throw new Error(`inboxkitten html ${response.status}`);
  }
  return response.text();
}

async function fetchMeta(region: string, key: string): Promise<MessageMeta> {
  const url = `${API_BASE}/getKey?region=${encodeURIComponent(region)}&key=${encodeURIComponent(key)}`;
  return fetchJSON<MessageMeta>(url);
}

async function fetchDetail(row: ListItem, recipient: string): Promise<Record<string, unknown>> {
  const region = String(row.storage?.region || '').trim();
  const key = String(row.storage?.key || '').trim();
  const base = {
    id: key,
    messageId: key,
    from: row.message?.headers?.from || row.envelope?.sender || '',
    to: row.recipient || row.envelope?.targets || recipient,
    subject: row.message?.headers?.subject || '',
    timestamp: row.timestamp,
  };
  if (!region || !key) return base;

  const [meta, html] = await Promise.all([fetchMeta(region, key), fetchHtml(region, key)]);
  return {
    ...base,
    from: meta.name || base.from,
    to: meta.recipients || base.to,
    subject: meta.subject || base.subject,
    text: htmlToText(html),
    html,
  };
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const local = randomLocal();
  return { channel: CHANNEL, email: `${local}@${DOMAIN}` };
}

export async function getEmails(email: string): Promise<Email[]> {
  const local = localPart(email);
  if (!local) throw new Error('inboxkitten: empty email');

  const rows = await fetchJSON<ListItem[]>(`${API_BASE}/list?recipient=${encodeURIComponent(local)}`);
  if (!Array.isArray(rows)) return [];

  const emails: Email[] = [];
  for (const row of rows) {
    try {
      emails.push(normalizeEmail(await fetchDetail(row, email), email));
    } catch {
      emails.push(normalizeEmail({
        id: row.storage?.key || '',
        from: row.message?.headers?.from || row.envelope?.sender || '',
        to: row.recipient || row.envelope?.targets || email,
        subject: row.message?.headers?.subject || '',
        timestamp: row.timestamp,
      }, email));
    }
  }
  return emails;
}
