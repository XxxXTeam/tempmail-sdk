import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'cleantempmail';
const BASE_URL = 'https://cleantempmail.com/api';
const DEFAULT_API_KEY = 'ct-test';
const DEFAULT_HEADERS: Record<string, string> = {
  Accept: 'application/json',
  'User-Agent': 'Mozilla/5.0',
};

function apiKey(): string {
  return process.env.CLEANTEMPMAIL_API_KEY?.trim() || DEFAULT_API_KEY;
}

function jsonHeaders(): Record<string, string> {
  return {
    ...DEFAULT_HEADERS,
    'X-API-Key': apiKey(),
  };
}

async function fetchJson(url: string): Promise<unknown> {
  const response = await fetchWithTimeout(url, {
    method: 'GET',
    headers: jsonHeaders(),
  });
  if (!response.ok) {
    throw new Error(`cleantempmail: ${response.status}`);
  }
  return response.json();
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const root = await fetchJson(`${BASE_URL}/generate-email`) as {
    success?: boolean;
    data?: { email?: string; mailbox?: string };
  };
  const data = root && typeof root === 'object' ? root.data || {} : {};
  let email = String(data.email || '').trim();
  if (!email && String(data.mailbox || '').trim()) {
    email = String(data.mailbox).trim();
  }
  if (!email || !email.includes('@')) {
    throw new Error('cleantempmail: invalid generate-email response');
  }
  return { channel: CHANNEL, email };
}

export async function getEmails(email: string): Promise<Email[]> {
  const address = String(email || '').trim();
  if (!address) throw new Error('cleantempmail: empty email');

  const root = await fetchJson(`${BASE_URL}/emails?email=${encodeURIComponent(address)}`) as {
    success?: boolean;
    data?: { emails?: Array<Record<string, unknown>> };
  };
  const rows = Array.isArray(root?.data?.emails) ? root.data.emails : [];
  const emails: Email[] = [];
  for (const row of rows) {
    if (!row || typeof row !== 'object' || Array.isArray(row)) continue;
    emails.push(normalizeEmail(row as Record<string, unknown>, address));
  }
  return emails;
}
