import { EmailInfo, Email, Channel } from '../types';

const CHANNEL: Channel = 'linshi-email';
const BASE_URL = 'https://www.linshi-email.com/api/v1';
const API_KEY = '552562b8524879814776e52bc8de5c9f';

const DEFAULT_HEADERS = {
  'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36',
  'Content-Type': 'application/json',
  'Origin': 'https://www.linshi-email.com',
  'Referer': 'https://www.linshi-email.com/',
  'sec-ch-ua': '"Microsoft Edge";v="143", "Chromium";v="143", "Not A(Brand";v="24"',
  'sec-ch-ua-mobile': '?0',
  'sec-ch-ua-platform': '"Windows"',
  'DNT': '1',
};

export async function generateEmail(): Promise<EmailInfo> {
  const response = await fetch(`${BASE_URL}/email/${API_KEY}`, {
    method: 'POST',
    headers: DEFAULT_HEADERS,
    body: JSON.stringify({}),
  });

  if (!response.ok) {
    throw new Error(`Failed to generate email: ${response.status}`);
  }

  const data = await response.json();
  
  if (data.status !== 'ok') {
    throw new Error('Failed to generate email');
  }

  return {
    channel: CHANNEL,
    email: data.data.email,
    expiresAt: data.data.expired,
  };
}

export async function getEmails(email: string): Promise<Email[]> {
  const encodedEmail = encodeURIComponent(email);
  const timestamp = Date.now();
  const response = await fetch(`${BASE_URL}/refreshmessage/${API_KEY}/${encodedEmail}?t=${timestamp}`, {
    method: 'GET',
    headers: DEFAULT_HEADERS,
  });

  if (!response.ok) {
    throw new Error(`Failed to get emails: ${response.status}`);
  }

  const data = await response.json();
  
  if (data.status !== 'ok') {
    throw new Error('Failed to get emails');
  }

  return data.list || [];
}
