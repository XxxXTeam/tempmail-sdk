import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'segamail';
const BASE = 'https://segamail.com';

const DEFAULT_HEADERS: Record<string, string> = {
  Accept: 'application/json, text/plain, */*',
  'Accept-Language': 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
  'Cache-Control': 'no-cache',
  DNT: '1',
  Pragma: 'no-cache',
  Referer: `${BASE}/en`,
  'Content-Type': 'application/x-www-form-urlencoded',
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

/**
 * 创建 segamail.com 临时邮箱
 * POST https://segamail.com/en/getEmailAddress
 * 响应: {"id":"591815","address":"xxx@segamail.com","creation_time":"...","recover_key":"LSJFEYQ591815"}
 * 使用 recover_key 作为 token
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const response = await fetchWithTimeout(`${BASE}/en/getEmailAddress`, {
    method: 'POST',
    headers: DEFAULT_HEADERS,
  });
  if (!response.ok) {
    throw new Error(`segamail: 创建邮箱失败 HTTP ${response.status}`);
  }
  const data = (await response.json()) as {
    id?: string;
    address?: string;
    creation_time?: string;
    recover_key?: string;
  };
  if (!data.address) {
    throw new Error('segamail: 创建邮箱返回数据中无邮箱地址');
  }
  return {
    channel: CHANNEL,
    email: data.address,
    token: data.recover_key || '',
    createdAt: data.creation_time || new Date().toISOString(),
  };
}

/**
 * 获取 segamail.com 邮件列表
 * POST https://segamail.com/en/getInbox (依赖 session cookie，SDK 共享客户端自动处理)
 * 响应: [{"from":"...","date":"...","subject":"...","body":"..."}]
 */
export async function getEmails(_token: string, email: string): Promise<Email[]> {
  if (!email?.trim()) {
    throw new Error('segamail: 邮箱地址为空');
  }
  const addr = email.trim();
  const response = await fetchWithTimeout(`${BASE}/en/getInbox`, {
    method: 'POST',
    headers: DEFAULT_HEADERS,
  });
  if (!response.ok) {
    throw new Error(`segamail: 获取邮件失败 HTTP ${response.status}`);
  }
  const data = await response.json();
  if (!Array.isArray(data)) {
    return [];
  }
  return data.map((raw: unknown) => normalizeEmail(raw, addr));
}
