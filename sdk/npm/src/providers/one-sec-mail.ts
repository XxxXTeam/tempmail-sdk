import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = '1sec-mail';
const BASE_URL = 'https://tmaily.com/';

function setCookieLines(headers: Headers): string[] {
  const h = headers as Headers & { getSetCookie?: () => string[] };
  if (typeof h.getSetCookie === 'function') return h.getSetCookie();
  const one = headers.get('set-cookie');
  return one ? one.split(/,(?=[^ ;]+=)/) : [];
}

function extractCookie(headers: Headers): string {
  for (const line of setCookieLines(headers)) {
    const first = line.split(';')[0]?.trim() || '';
    if (first.startsWith('TMaily_sid=')) return first;
  }
  return '';
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const resp = await fetchWithTimeout(`${BASE_URL}generate`, {
    headers: { Accept: 'application/json', 'User-Agent': 'Mozilla/5.0' },
  });
  if (!resp.ok) throw new Error(`1sec-mail generate: ${resp.status}`);
  const cookie = extractCookie(resp.headers);
  if (!cookie) throw new Error('1sec-mail: 未获取到会话 Cookie');
  const data = (await resp.json()) as { address?: string };
  if (!data || typeof data.address !== 'string' || !data.address.includes('@')) {
    throw new Error('1sec-mail: 无效的邮箱响应');
  }
  return {
    channel: CHANNEL,
    email: data.address.trim(),
    token: cookie,
  };
}

export async function getEmails(token: string, email: string): Promise<Email[]> {
  const address = (email || '').trim();
  if (!address) throw new Error('1sec-mail: 邮箱地址为空');
  const resp = await fetchWithTimeout(
    `${BASE_URL}emails?address=${encodeURIComponent(address)}`,
    {
      headers: {
        Accept: 'application/json',
        Cookie: token,
        'User-Agent': 'Mozilla/5.0',
      },
    },
  );
  if (!resp.ok) throw new Error(`1sec-mail emails: ${resp.status}`);
  const rows = (await resp.json()) as Array<Record<string, unknown>>;
  if (!Array.isArray(rows)) throw new Error('1sec-mail: 无效的邮件列表响应');
  return rows.map((raw) => normalizeEmail(raw, address));
}
