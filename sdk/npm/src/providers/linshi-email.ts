import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';
import { randomSyntheticLinshiKey } from './linshi-token';

const CHANNEL: Channel = 'linshi-email';
const BASE_URL = 'https://www.linshi-email.com/api/v1';

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

export async function generateEmail(): Promise<InternalEmailInfo> {
  const { apiPathKey } = randomSyntheticLinshiKey();
  const response = await fetchWithTimeout(`${BASE_URL}/email/${apiPathKey}`, {
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

  const d = data.data;
  const raw =
    (typeof d?.email === 'string' && d.email) ||
    (typeof d?.mail === 'string' && d.mail) ||
    (typeof d?.address === 'string' && d.address) ||
    '';
  const email = String(raw).trim();

  if (!email || !email.includes('@')) {
    throw new Error(
      'linshi-email: API 未返回有效邮箱地址（data 为空或缺 email 字段，常见于频率限制：每小时约 10 个 / 每天约 20 个）',
    );
  }

  return {
    channel: CHANNEL,
    email,
    expiresAt: d?.expired,
    token: apiPathKey,
  };
}

export async function getEmails(email: string, apiPathKey: string): Promise<Email[]> {
  const encodedEmail = encodeURIComponent(email);
  const timestamp = Date.now();
  const response = await fetchWithTimeout(`${BASE_URL}/refreshmessage/${apiPathKey}/${encodedEmail}?t=${timestamp}`, {
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

  const rawEmails = data.list || [];
  return rawEmails.map((raw: any) => normalizeEmail(raw, email));
}
