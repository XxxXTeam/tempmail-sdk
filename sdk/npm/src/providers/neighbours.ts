import { randomInt } from 'crypto';
import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'neighbours';
const API_BASE = 'https://neighbours.sh/api/v1';

type DomainListResponse = {
  data?: {
    domains?: string[];
  };
};

type InboxAddress = {
  address?: string;
};

type InboxGroup = {
  value?: InboxAddress[];
  text?: string;
  html?: string;
};

type InboxRow = {
  uid?: number | string;
  from?: InboxGroup | InboxAddress[];
  to?: InboxGroup | InboxAddress[] | string[];
  subject?: string;
  text?: string;
  snippet?: string;
  html?: string;
  date?: string;
  attachments?: unknown[];
};

type InboxListResponse = {
  data?: InboxRow[];
};

type InboxDetailResponse = {
  data?: InboxRow;
};

const HEADERS: Record<string, string> = {
  Accept: 'application/json',
  'User-Agent':
    'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36',
};

function randomLocal(length = 16): string {
  const chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
  let out = 'sdk';
  for (let i = 0; i < length; i++) {
    out += chars[randomInt(chars.length)];
  }
  return out;
}

async function fetchJson<T>(path: string, allowNotFound = false): Promise<T | null> {
  const response = await fetchWithTimeout(`${API_BASE}${path}`, {
    headers: HEADERS,
  });
  if (allowNotFound && response.status === 404) {
    return null;
  }
  if (!response.ok) {
    throw new Error(`neighbours http ${response.status}`);
  }
  return response.json() as Promise<T>;
}

async function listDomains(): Promise<string[]> {
  const data = await fetchJson<DomainListResponse>('/config/domains');
  const domains = (data?.data?.domains || [])
    .map((item) => item.trim().toLowerCase())
    .filter(Boolean);
  if (domains.length === 0) {
    throw new Error('neighbours: domain list is empty');
  }
  return domains;
}

function pickDomain(domains: string[], preferred?: string | null): string {
  if (preferred && preferred.trim()) {
    const wanted = preferred.trim().toLowerCase();
    const hit = domains.find((item) => item === wanted);
    if (!hit) {
      throw new Error(`neighbours: unsupported domain ${preferred}`);
    }
    return hit;
  }
  return domains[randomInt(domains.length)];
}

function firstAddress(value: unknown): string {
  if (!value) return '';
  if (Array.isArray(value)) {
    for (const item of value) {
      const hit = firstAddress(item);
      if (hit) return hit;
    }
    return '';
  }
  if (typeof value === 'string') {
    return value.trim();
  }
  if (typeof value === 'object') {
    const obj = value as Record<string, unknown>;
    if (typeof obj.address === 'string' && obj.address.trim()) {
      return obj.address.trim();
    }
    if (typeof obj.text === 'string' && obj.text.trim() && obj.text.includes('@')) {
      return obj.text.trim();
    }
    return firstAddress(obj.value);
  }
  return '';
}

function flattenMessage(raw: InboxRow, recipient: string): Record<string, unknown> {
  return {
    id: raw.uid == null ? '' : String(raw.uid),
    from: firstAddress(raw.from),
    to: firstAddress(raw.to) || recipient,
    subject: raw.subject || '',
    text: raw.text || raw.snippet || '',
    html: raw.html || '',
    date: raw.date || '',
    isRead: false,
    attachments: Array.isArray(raw.attachments) ? raw.attachments : [],
  };
}

async function fetchDetail(address: string, uid: string): Promise<InboxRow | null> {
  try {
    const data = await fetchJson<InboxDetailResponse>(
      `/inbox/${encodeURIComponent(address)}/${encodeURIComponent(uid)}`,
    );
    if (!data) {
      return null;
    }
    return data.data && typeof data.data === 'object' ? data.data : null;
  } catch {
    return null;
  }
}

export async function generateEmail(domain?: string | null): Promise<InternalEmailInfo> {
  const domains = await listDomains();
  const selectedDomain = pickDomain(domains, domain);
  return {
    channel: CHANNEL,
    email: `${randomLocal()}@${selectedDomain}`,
  };
}

export async function getEmails(email: string): Promise<Email[]> {
  const address = String(email || '').trim();
  if (!address) {
    throw new Error('neighbours: empty email');
  }

  const list = await fetchJson<InboxListResponse>(`/inbox/${encodeURIComponent(address)}`, true);
  if (!list) {
    return [];
  }
  const rows = Array.isArray(list.data) ? list.data : [];
  const emails: Email[] = [];
  for (const row of rows) {
    const uid = String(row.uid ?? '').trim();
    const detail = uid ? await fetchDetail(address, uid) : null;
    emails.push(normalizeEmail(flattenMessage(detail || row, address), address));
  }
  return emails;
}
