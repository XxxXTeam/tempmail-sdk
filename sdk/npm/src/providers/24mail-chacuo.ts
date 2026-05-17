import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = '24mail-chacuo';
const BASE_URL = 'http://24mail.chacuo.net';

const DEFAULT_HEADERS: Record<string, string> = {
  'Accept': 'application/json, text/javascript, */*; q=0.01',
  'Accept-Language': 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
  'Content-Type': 'application/x-www-form-urlencoded; charset=UTF-8',
  'Origin': BASE_URL,
  'Referer': `${BASE_URL}/`,
  'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
  'x-requested-with': 'XMLHttpRequest',
};

/** 可用域名列表 */
const DOMAINS = ['chacuo.net', '027168.com'];

/**
 * 生成随机字母数字用户名
 */
function randomLocal(length: number): string {
  const chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
  let result = '';
  for (let i = 0; i < length; i++) {
    result += chars.charAt(Math.floor(Math.random() * chars.length));
  }
  return result;
}

/**
 * 24mail.chacuo.net API 响应结构
 */
interface ChacuoResponse {
  status: number;
  info: string;
  data: Array<{
    stat?: Record<string, unknown>;
    user?: {
      UID?: string;
      USER?: string;
      STATUS?: string;
      [key: string]: unknown;
    };
    list?: Array<{
      MID?: string;
      USER?: string;
      FROM?: string;
      SUBJECT?: string;
      CONTENT?: string;
      TIME?: string;
      [key: string]: unknown;
    }>;
  }>;
}

/**
 * 创建 24mail-chacuo 临时邮箱
 * 向 24mail.chacuo.net 发送 POST 请求注册随机邮箱
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const name = randomLocal(10);
  const domain = DOMAINS[Math.floor(Math.random() * DOMAINS.length)];

  const response = await fetchWithTimeout(BASE_URL + '/', {
    method: 'POST',
    headers: DEFAULT_HEADERS,
    body: `data=${name}%40${domain}&type=refresh&arg=`,
  });

  if (!response.ok) {
    throw new Error(`24mail-chacuo: 创建邮箱失败 HTTP ${response.status}`);
  }

  const rawText = await response.text();
  let data: ChacuoResponse;
  try {
    data = JSON.parse(rawText) as ChacuoResponse;
  } catch {
    throw new Error(`24mail-chacuo: 返回非 JSON: ${rawText.slice(0, 120)}`);
  }

  if (data.status !== 1) {
    throw new Error(`24mail-chacuo: 创建失败 status=${data.status} info=${data.info}`);
  }

  const email = `${name}@${domain}`;

  return {
    channel: CHANNEL,
    email,
    /* token 存储用户名和域名，供获取邮件时使用 */
    token: `${name}@${domain}`,
  };
}

/**
 * 获取 24mail-chacuo 邮件列表
 * 轮询同一接口获取邮件
 */
export async function getEmails(email: string, _token?: string): Promise<Email[]> {
  const atIdx = email.indexOf('@');
  const name = atIdx > 0 ? email.slice(0, atIdx) : email;
  const domain = atIdx > 0 ? email.slice(atIdx + 1) : DOMAINS[0];

  const response = await fetchWithTimeout(BASE_URL + '/', {
    method: 'POST',
    headers: DEFAULT_HEADERS,
    body: `data=${name}%40${domain}&type=refresh&arg=`,
  });

  if (!response.ok) {
    throw new Error(`24mail-chacuo: 获取邮件失败 HTTP ${response.status}`);
  }

  const rawText = await response.text();
  let data: ChacuoResponse;
  try {
    data = JSON.parse(rawText) as ChacuoResponse;
  } catch {
    throw new Error(`24mail-chacuo: 返回非 JSON: ${rawText.slice(0, 120)}`);
  }

  if (data.status !== 1 || !data.data || !data.data.length) {
    return [];
  }

  const entry = data.data[0];
  const list = entry.list || [];

  return list.map((raw) =>
    normalizeEmail(
      {
        id: raw.MID || '',
        from: raw.FROM || '',
        to: email,
        subject: raw.SUBJECT || '',
        body: raw.CONTENT || '',
        date: raw.TIME || '',
        isRead: false,
        attachments: [],
      },
      email,
    ),
  );
}
