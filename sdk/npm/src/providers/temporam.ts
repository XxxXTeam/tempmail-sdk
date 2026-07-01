import { randomInt } from 'crypto';
import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'temporam';
const ORIGIN = 'https://temporam.com';

const JSON_HEADERS = {
  Accept: 'application/json',
  'Accept-Encoding': 'gzip, deflate, br',
  'User-Agent':
    'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36',
};

type DomainItem = {
  domain?: string;
};

type DomainsResponse = {
  data?: DomainItem[];
};

type EmailsResponse = {
  data?: any[];
};

type EmailDetailResponse = {
  data?: any;
};

function randomLocal(length = 18): string {
  const chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
  let out = 'sdk';
  for (let i = 0; i < length; i++) {
    out += chars[randomInt(chars.length)];
  }
  return out;
}

async function getJson<T>(path: string): Promise<T> {
  const response = await fetchWithTimeout(`${ORIGIN}${path}`, {
    headers: JSON_HEADERS,
  });
  if (!response.ok) {
    throw new Error(`temporam request failed: ${response.status}`);
  }
  return await response.json() as T;
}

async function listDomains(): Promise<string[]> {
  const data = await getJson<DomainsResponse>('/api/domains');
  const domains = (data.data ?? [])
    .map(item => (item.domain ?? '').trim())
    .filter(Boolean);
  if (domains.length === 0) {
    throw new Error('temporam: domain list is empty');
  }
  return domains;
}

function normalizeTemporamEmail(raw: any, email: string): Email {
  return normalizeEmail({
    ...raw,
    id: raw?.id || raw?.uuid || '',
    from: raw?.fromEmail || raw?.from || '',
    to: raw?.toEmail || raw?.to || email,
    date: raw?.createdAt || raw?.created_at || raw?.date || '',
  }, email);
}

export async function generateEmail(domain?: string | null): Promise<InternalEmailInfo> {
  const domains = await listDomains();
  const selectedDomain = domain
    ? domains.find(item => item === domain)
    : domains[randomInt(domains.length)];
  if (!selectedDomain) {
    throw new Error(`temporam: unsupported domain ${domain}`);
  }

  return {
    channel: CHANNEL,
    email: `${randomLocal()}@${selectedDomain}`,
  };
}

export async function getEmails(email: string): Promise<Email[]> {
  const list = await getJson<EmailsResponse>(`/api/emails?email=${encodeURIComponent(email)}&limit=50`);
  const rows = Array.isArray(list.data) ? list.data : [];
  const out: Email[] = [];

  for (const row of rows) {
    const id = row?.id || row?.uuid || '';
    if (!id) {
      out.push(normalizeTemporamEmail(row, email));
      continue;
    }

    try {
      const detail = await getJson<EmailDetailResponse>(`/api/emails/${encodeURIComponent(String(id))}`);
      out.push(normalizeTemporamEmail(detail.data || row, email));
    } catch {
      out.push(normalizeTemporamEmail(row, email));
    }
  }

  return out;
}
