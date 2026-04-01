import { InternalEmailInfo, Email, Channel } from '../types';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'maildrop';
const BASE = 'https://maildrop.cx';
const EXCLUDED_SUFFIX = 'transformer.edu.kg';
const DEFAULT_HEADERS: Record<string, string> = {
  Accept: 'application/json, text/plain, */*',
  'Accept-Language': 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
  'Cache-Control': 'no-cache',
  DNT: '1',
  Pragma: 'no-cache',
  Referer: 'https://maildrop.cx/zh-cn/app',
  'sec-ch-ua': '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
  'sec-ch-ua-mobile': '?0',
  'sec-ch-ua-platform': '"Windows"',
  'sec-fetch-dest': 'empty',
  'sec-fetch-mode': 'cors',
  'sec-fetch-site': 'same-origin',
  'User-Agent':
    'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
  'x-requested-with': 'XMLHttpRequest',
};

function randomLocalPart(length = 10): string {
  const chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
  let s = '';
  for (let i = 0; i < length; i++) {
    s += chars.charAt(Math.floor(Math.random() * chars.length));
  }
  return s;
}

async function fetchSuffixes(): Promise<string[]> {
  const response = await fetchWithTimeout(`${BASE}/api/suffixes.php`, {
    method: 'GET',
    headers: DEFAULT_HEADERS,
  });
  if (!response.ok) {
    throw new Error(`maildrop: 获取后缀列表失败 HTTP ${response.status}`);
  }
  const data = (await response.json()) as unknown;
  if (!Array.isArray(data)) {
    throw new Error('maildrop: 后缀列表格式无效');
  }
  const ex = EXCLUDED_SUFFIX.toLowerCase();
  const list = data
    .filter((x): x is string => typeof x === 'string' && x.trim().length > 0)
    .map((d) => d.trim())
    .filter((d) => d.toLowerCase() !== ex);
  if (list.length === 0) {
    throw new Error('maildrop: 无可用域名');
  }
  return list;
}

function pickSuffix(suffixes: string[], preferred?: string | null): string {
  if (preferred && typeof preferred === 'string') {
    const p = preferred.trim().toLowerCase();
    const hit = suffixes.find((d) => d.toLowerCase() === p);
    if (hit) return hit;
  }
  return suffixes[Math.floor(Math.random() * suffixes.length)];
}

function cxDateToIso(dateStr: string): string {
  const s = dateStr?.trim() ?? '';
  if (/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/.test(s)) {
    return s.replace(' ', 'T') + 'Z';
  }
  return s;
}

interface EmailsApiRow {
  id?: number | string;
  from_addr?: string;
  subject?: string;
  date?: string;
  isRead?: number | boolean;
  description?: string;
}

interface EmailsApiResponse {
  emails?: EmailsApiRow[];
}

export async function generateEmail(domain?: string | null): Promise<InternalEmailInfo> {
  const suffixes = await fetchSuffixes();
  const dom = pickSuffix(suffixes, domain ?? undefined);
  const local = randomLocalPart();
  const email = `${local}@${dom}`;
  return {
    channel: CHANNEL,
    email,
    token: email,
  };
}

export async function getEmails(_token: string, email: string): Promise<Email[]> {
  const addr = email?.trim();
  if (!addr) {
    throw new Error('maildrop: 邮箱地址为空');
  }
  const qs = new URLSearchParams({ addr, page: '1', limit: '20' });
  const response = await fetchWithTimeout(`${BASE}/api/emails.php?${qs}`, {
    method: 'GET',
    headers: DEFAULT_HEADERS,
  });
  if (!response.ok) {
    throw new Error(`maildrop: 获取邮件失败 HTTP ${response.status}`);
  }
  const data = (await response.json()) as EmailsApiResponse;
  const rows = Array.isArray(data.emails) ? data.emails : [];
  return rows.map((item) => {
    const desc = item.description?.trim() ?? '';
    const isRead = item.isRead === true || item.isRead === 1;
    return {
      id: String(item.id ?? ''),
      from: item.from_addr?.trim() ?? '',
      to: addr,
      subject: item.subject?.trim() ?? '',
      text: desc,
      html: '',
      date: cxDateToIso(item.date ?? ''),
      isRead,
      attachments: [],
    };
  });
}
