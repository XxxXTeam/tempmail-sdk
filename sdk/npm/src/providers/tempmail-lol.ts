import { EmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';

const CHANNEL: Channel = 'tempmail-lol';
const BASE_URL = 'https://api.tempmail.lol/v2';

const DEFAULT_HEADERS = {
  'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36',
  'Content-Type': 'application/json',
  'Origin': 'https://tempmail.lol',
  'sec-ch-ua': '"Microsoft Edge";v="143", "Chromium";v="143", "Not A(Brand";v="24"',
  'sec-ch-ua-mobile': '?0',
  'sec-ch-ua-platform': '"Windows"',
  'DNT': '1',
};

export async function generateEmail(domain: string | null = null): Promise<EmailInfo> {
  const response = await fetch(`${BASE_URL}/inbox/create`, {
    method: 'POST',
    headers: DEFAULT_HEADERS,
    body: JSON.stringify({ domain, captcha: null }),
  });

  if (!response.ok) {
    throw new Error(`Failed to generate email: ${response.status}`);
  }

  const data = await response.json();
  
  if (!data.address || !data.token) {
    throw new Error('Failed to generate email');
  }

  return {
    channel: CHANNEL,
    email: data.address,
    token: data.token,
  };
}

export async function getEmails(token: string, recipientEmail: string = ''): Promise<Email[]> {
  const response = await fetch(`${BASE_URL}/inbox?token=${encodeURIComponent(token)}`, {
    method: 'GET',
    headers: DEFAULT_HEADERS,
  });

  if (!response.ok) {
    throw new Error(`Failed to get emails: ${response.status}`);
  }

  const data = await response.json();
  
  const rawEmails = data.emails || [];
  return rawEmails.map((raw: any) => normalizeEmail(raw, recipientEmail));
}
