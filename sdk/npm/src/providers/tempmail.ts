import { EmailInfo, Email, Channel } from '../types';

const CHANNEL: Channel = 'tempmail';
const BASE_URL = 'https://api.tempmail.ing/api';

const DEFAULT_HEADERS = {
  'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36',
  'Content-Type': 'application/json',
  'Referer': 'https://tempmail.ing/',
  'sec-ch-ua': '"Microsoft Edge";v="143", "Chromium";v="143", "Not A(Brand";v="24"',
  'sec-ch-ua-mobile': '?0',
  'sec-ch-ua-platform': '"Windows"',
  'DNT': '1',
};

export async function generateEmail(duration: number = 30): Promise<EmailInfo> {
  const response = await fetch(`${BASE_URL}/generate`, {
    method: 'POST',
    headers: DEFAULT_HEADERS,
    body: JSON.stringify({ duration }),
  });

  if (!response.ok) {
    throw new Error(`Failed to generate email: ${response.status}`);
  }

  const data = await response.json();
  
  if (!data.success) {
    throw new Error('Failed to generate email');
  }

  return {
    channel: CHANNEL,
    email: data.email.address,
    expiresAt: data.email.expiresAt,
    createdAt: data.email.createdAt,
  };
}

export async function getEmails(email: string): Promise<Email[]> {
  const encodedEmail = encodeURIComponent(email);
  const response = await fetch(`${BASE_URL}/emails/${encodedEmail}`, {
    method: 'GET',
    headers: DEFAULT_HEADERS,
  });

  if (!response.ok) {
    throw new Error(`Failed to get emails: ${response.status}`);
  }

  const data = await response.json();
  
  if (!data.success) {
    throw new Error('Failed to get emails');
  }

  return data.emails || [];
}
