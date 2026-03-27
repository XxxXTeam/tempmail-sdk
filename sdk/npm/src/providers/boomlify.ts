import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'boomlify';
const BASE_URL = 'https://v1.boomlify.com';

const DEFAULT_HEADERS = {
  'Accept': 'application/json, text/plain, */*',
  'Accept-Language': 'zh',
  'Origin': 'https://boomlify.com',
  'Referer': 'https://boomlify.com/',
  'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
  'sec-ch-ua': '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
  'sec-ch-ua-mobile': '?0',
  'sec-ch-ua-platform': '"Windows"',
  'sec-fetch-dest': 'empty',
  'sec-fetch-mode': 'cors',
  'sec-fetch-site': 'same-site',
  'x-user-language': 'zh',
};

interface BoomlifyDomain {
  id: string;
  domain: string;
  created_at: string;
  is_premium: number;
  is_edu: number;
  expires_at: string;
  is_active: number;
}

interface BoomlifyEmail {
  id: string;
  temp_email_id: string;
  from_email: string;
  from_name: string;
  subject: string;
  body_html: string;
  body_text: string;
  received_at: string;
  temp_email: string;
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
  const response = await fetchWithTimeout(`${BASE_URL}/domains/public`, {
    method: 'GET',
    headers: DEFAULT_HEADERS,
  });

  if (!response.ok) {
    throw new Error(`Failed to get domains: ${response.status}`);
  }

  const data: BoomlifyDomain[] = await response.json();
  return data
    .filter(d => d.is_active === 1)
    .map(d => d.domain);
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const domains = await getDomains();

  if (domains.length === 0) {
    throw new Error('Failed to generate email: no domains available');
  }

  const domain = domains[Math.floor(Math.random() * domains.length)];
  const localPart = randomString(8);

  return {
    channel: CHANNEL,
    email: `${localPart}@${domain}`,
  };
}

export async function getEmails(email: string): Promise<Email[]> {
  const response = await fetchWithTimeout(`${BASE_URL}/emails/public/${encodeURIComponent(email)}`, {
    method: 'GET',
    headers: DEFAULT_HEADERS,
  });

  if (!response.ok) {
    throw new Error(`Failed to get emails: ${response.status}`);
  }

  const data: BoomlifyEmail[] = await response.json();

  return data.map((raw) => normalizeEmail({
    id: raw.id,
    from: raw.from_email || raw.from_name || '',
    to: email,
    subject: raw.subject || '',
    text: raw.body_text || '',
    html: raw.body_html || '',
    date: raw.received_at || '',
    isRead: false,
  }, email));
}