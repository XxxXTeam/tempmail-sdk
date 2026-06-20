import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'temp-mail-io';
const BASE_URL = 'https://api.internal.temp-mail.io/api/v3';
const PAGE_URL = 'https://temp-mail.io/en';

let cachedCorsHeader: string | null = null;

async function fetchCorsHeader(): Promise<string> {
  if (cachedCorsHeader) return cachedCorsHeader;

  try {
    const response = await fetchWithTimeout(PAGE_URL, {
      headers: {
        'User-Agent':
          'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36',
      },
    });
    const html = await response.text();
    const match = html.match(/mobileTestingHeader\s*:\s*"([^"]+)"/);
    if (match) {
      cachedCorsHeader = match[1];
      return cachedCorsHeader;
    }
  } catch {
    /* fallback below */
  }

  cachedCorsHeader = '1';
  return cachedCorsHeader;
}

async function getApiHeaders(): Promise<Record<string, string>> {
  const corsHeader = await fetchCorsHeader();
  return {
    'Content-Type': 'application/json',
    'Application-Name': 'web',
    'Application-Version': '4.0.0',
    'X-CORS-Header': corsHeader,
    'User-Agent':
      'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0',
    origin: 'https://temp-mail.io',
    referer: 'https://temp-mail.io/',
  };
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const headers = await getApiHeaders();
  const response = await fetchWithTimeout(`${BASE_URL}/email/new`, {
    method: 'POST',
    headers,
    body: JSON.stringify({ min_name_length: 10, max_name_length: 10 }),
  });

  if (!response.ok) {
    throw new Error(`temp-mail-io generate: ${response.status}`);
  }

  const data = await response.json();
  if (!data.email || !data.token) {
    throw new Error('temp-mail-io: invalid generate response');
  }

  return {
    channel: CHANNEL,
    email: data.email,
    token: data.token,
  };
}

export async function getEmails(email: string): Promise<Email[]> {
  const headers = await getApiHeaders();
  const response = await fetchWithTimeout(`${BASE_URL}/email/${email}/messages`, {
    method: 'GET',
    headers,
  });

  if (!response.ok) {
    throw new Error(`temp-mail-io messages: ${response.status}`);
  }

  const data = await response.json();
  const rawEmails = Array.isArray(data) ? data : [];
  return rawEmails.map((raw: any) => normalizeEmail(raw, email));
}
