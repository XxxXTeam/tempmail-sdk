import * as https from 'https';
import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';

const CHANNEL: Channel = 'uncorreotemporal';
const HOST = 'uncorreotemporal.com';
const ORIGIN = `https://${HOST}`;
const IPV4_AGENT = new https.Agent({ family: 4 });

interface CreateResponse {
  address?: string;
  expires_at?: string;
  session_token?: string;
}

interface ListMessage {
  id?: string;
  from_address?: string;
  to_address?: string;
  subject?: string;
  received_at?: string;
  is_read?: boolean;
  has_attachments?: boolean;
}

interface DetailMessage extends ListMessage {
  body_text?: string;
  body_html?: string;
  attachments?: unknown[];
}

const HEADERS: Record<string, string> = {
  Accept: 'application/json',
  Origin: ORIGIN,
  Referer: `${ORIGIN}/`,
  'User-Agent': 'Mozilla/5.0',
};

function requestJSON<T>(method: 'GET' | 'POST', path: string, token?: string): Promise<T> {
  const headers: Record<string, string> = { ...HEADERS };
  if (method === 'POST') headers['Content-Type'] = 'application/json';
  if (token) headers['X-Session-Token'] = token;

  return new Promise<T>((resolve, reject) => {
    const req = https.request(
      {
        protocol: 'https:',
        hostname: HOST,
        path,
        method,
        headers,
        agent: IPV4_AGENT,
        timeout: 15000,
      },
      (res) => {
        const chunks: Buffer[] = [];
        res.on('data', (chunk: Buffer) => chunks.push(chunk));
        res.on('end', () => {
          const body = Buffer.concat(chunks).toString('utf8');
          const status = res.statusCode || 0;
          if (status < 200 || status >= 300) {
            reject(new Error(`uncorreotemporal http ${status}`));
            return;
          }
          try {
            resolve(JSON.parse(body) as T);
          } catch (err) {
            reject(err);
          }
        });
      },
    );
    req.on('timeout', () => req.destroy(new Error('uncorreotemporal timeout')));
    req.on('error', reject);
    req.end();
  });
}

function flatten(raw: DetailMessage | ListMessage, recipient: string): Record<string, unknown> {
  return {
    id: raw.id || '',
    from: raw.from_address || '',
    to: raw.to_address || recipient,
    subject: raw.subject || '',
    text: (raw as DetailMessage).body_text || '',
    html: (raw as DetailMessage).body_html || '',
    date: raw.received_at || '',
    isRead: Boolean(raw.is_read),
    attachments: Array.isArray((raw as DetailMessage).attachments) ? (raw as DetailMessage).attachments : [],
  };
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const data = await requestJSON<CreateResponse>('POST', '/api/v1/mailboxes');
  const email = String(data.address || '').trim();
  const token = String(data.session_token || '').trim();
  if (!email || !email.includes('@') || !token) {
    throw new Error('uncorreotemporal: invalid mailbox response');
  }
  return {
    channel: CHANNEL,
    email,
    token,
    expiresAt: data.expires_at,
  };
}

async function fetchDetail(email: string, token: string, messageID: string): Promise<DetailMessage | null> {
  try {
    return await requestJSON<DetailMessage>(
      'GET',
      `/api/v1/mailboxes/${encodeURIComponent(email)}/messages/${encodeURIComponent(messageID)}`,
      token,
    );
  } catch {
    return null;
  }
}

export async function getEmails(token: string, email: string): Promise<Email[]> {
  const sessionToken = String(token || '').trim();
  const address = String(email || '').trim();
  if (!sessionToken) throw new Error('uncorreotemporal: empty session token');
  if (!address) throw new Error('uncorreotemporal: empty email');

  const rows = await requestJSON<ListMessage[]>(
    'GET',
    `/api/v1/mailboxes/${encodeURIComponent(address)}/messages?limit=50`,
    sessionToken,
  );
  const emails: Email[] = [];
  for (const row of Array.isArray(rows) ? rows : []) {
    const id = String(row.id || '').trim();
    const detail = id ? await fetchDetail(address, sessionToken, id) : null;
    emails.push(normalizeEmail(flatten(detail || row, address), address));
  }
  return emails;
}
