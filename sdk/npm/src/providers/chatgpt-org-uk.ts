import { EmailInfo, Email, Channel } from '../types';

const CHANNEL: Channel = 'chatgpt-org-uk';
const BASE_URL = 'https://mail.chatgpt.org.uk/api';

const DEFAULT_HEADERS = {
  'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36',
  'Content-Type': 'application/json',
  'Accept': '*/*',
  'Referer': 'https://mail.chatgpt.org.uk/',
  'sec-ch-ua': '"Microsoft Edge";v="143", "Chromium";v="143", "Not A(Brand";v="24"',
  'sec-ch-ua-mobile': '?0',
  'sec-ch-ua-platform': '"Windows"',
  'DNT': '1',
};

export async function generateEmail(): Promise<EmailInfo> {
  const response = await fetch(`${BASE_URL}/generate-email`, {
    method: 'GET',
    headers: DEFAULT_HEADERS,
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
    email: data.data.email,
  };
}

export async function getEmails(email: string): Promise<Email[]> {
  const encodedEmail = encodeURIComponent(email);
  const response = await fetch(`${BASE_URL}/emails?email=${encodedEmail}`, {
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

  return data.data?.emails || [];
}
