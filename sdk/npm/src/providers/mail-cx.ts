import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

/** Mail.cx REST API，见 https://docs.mail.cx */
const CHANNEL: Channel = 'mail-cx';
const BASE = 'https://api.mail.cx';

const DEFAULT_HEADERS: Record<string, string> = {
  Accept: 'application/json',
  Origin: 'https://mail.cx',
  Referer: 'https://mail.cx/',
  'User-Agent':
    'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36',
};

function jsonHeaders(): Record<string, string> {
  return { ...DEFAULT_HEADERS, 'Content-Type': 'application/json' };
}

function bearerHeaders(token: string): Record<string, string> {
  return { ...DEFAULT_HEADERS, Authorization: `Bearer ${token}` };
}

function randomString(length: number): string {
  const chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
  let result = '';
  for (let i = 0; i < length; i++) {
    result += chars[Math.floor(Math.random() * chars.length)];
  }
  return result;
}

async function getDomains(): Promise<string[]> {
  const response = await fetchWithTimeout(`${BASE}/api/domains`, {
    method: 'GET',
    headers: DEFAULT_HEADERS,
  });
  if (!response.ok) {
    throw new Error(`mail-cx domains: ${response.status}`);
  }
  const data = await response.json();
  const list = data?.domains;
  if (!Array.isArray(list)) return [];
  return list
    .map((d: { domain?: string }) => d?.domain)
    .filter((d: string | undefined): d is string => typeof d === 'string' && d.length > 0);
}

async function createAccount(address: string, password: string): Promise<{ address: string; token: string }> {
  const response = await fetchWithTimeout(`${BASE}/api/accounts`, {
    method: 'POST',
    headers: jsonHeaders(),
    body: JSON.stringify({ address, password }),
  });
  if (response.status !== 201) {
    const text = await response.text();
    throw new Error(`mail-cx create account: ${response.status} ${text}`);
  }
  const data = await response.json();
  if (!data?.token || !data?.address) {
    throw new Error('mail-cx: invalid account response');
  }
  return { address: data.address, token: data.token };
}

/**
 * @param preferredDomain - 若与列表中某域名匹配则仅用该域创建（不含 @ 前缀）
 */
export async function generateEmail(preferredDomain?: string | null): Promise<InternalEmailInfo> {
  let domains = await getDomains();
  if (domains.length === 0) {
    throw new Error('No available domains');
  }
  if (preferredDomain) {
    const want = preferredDomain.toLowerCase().replace(/^@/, '');
    if (domains.some((d) => d.toLowerCase() === want)) {
      domains = [domains.find((d) => d.toLowerCase() === want)!];
    }
  }

  const maxAttempts = 8;
  for (let attempt = 0; attempt < maxAttempts; attempt++) {
    const domain = domains[Math.floor(Math.random() * domains.length)];
    const username = randomString(12);
    const address = `${username}@${domain}`;
    const password = randomString(16);
    try {
      const acc = await createAccount(address, password);
      return {
        channel: CHANNEL,
        email: acc.address,
        token: acc.token,
        createdAt: new Date().toISOString(),
      };
    } catch (e: unknown) {
      const msg = String(e instanceof Error ? e.message : e);
      if (msg.includes('409') && attempt < maxAttempts - 1) {
        continue;
      }
      throw e;
    }
  }
  throw new Error('mail-cx: could not create account');
}

function flattenMessage(msg: Record<string, unknown>, recipientEmail: string): Record<string, unknown> {
  const id = String(msg.id ?? '');
  const attachments = Array.isArray(msg.attachments)
    ? (msg.attachments as Record<string, unknown>[]).map((a) => ({
        filename: a.filename || '',
        size: a.size,
        content_type: a.content_type,
        url: `${BASE}/api/messages/${id}/attachments/${a.index}`,
      }))
    : [];

  return {
    id,
    sender: msg.sender || '',
    from: msg.from || '',
    address: msg.address || recipientEmail,
    subject: msg.subject || '',
    preview_text: msg.preview_text,
    text_body: msg.text_body,
    html_body: msg.html_body,
    created_at: msg.created_at,
    attachments,
  };
}

export async function getEmails(token: string, email: string): Promise<Email[]> {
  const listResponse = await fetchWithTimeout(`${BASE}/api/messages?page=1`, {
    method: 'GET',
    headers: bearerHeaders(token),
  });
  if (!listResponse.ok) {
    throw new Error(`mail-cx list messages: ${listResponse.status}`);
  }
  const listData = await listResponse.json();
  const messages = Array.isArray(listData?.messages) ? listData.messages : [];
  if (messages.length === 0) {
    return [];
  }

  const detailPromises = messages.map(async (msg: Record<string, unknown>) => {
    const mid = msg.id;
    if (mid == null) {
      return flattenMessage(msg, email);
    }
    try {
      const detailResponse = await fetchWithTimeout(`${BASE}/api/messages/${mid}`, {
        method: 'GET',
        headers: bearerHeaders(token),
      });
      if (!detailResponse.ok) {
        return flattenMessage(msg, email);
      }
      const detail = await detailResponse.json();
      return flattenMessage(detail as Record<string, unknown>, email);
    } catch {
      return flattenMessage(msg, email);
    }
  });

  const flat = await Promise.all(detailPromises);
  return flat.map((raw) => normalizeEmail(raw, email));
}
