import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'pleasenospam';
const BASE = 'https://pleasenospam.email';

const DEFAULT_HEADERS: Record<string, string> = {
  Accept: 'application/json, text/plain, */*',
  'Accept-Language': 'zh-CN,zh;q=0.9,en;q=0.8',
  'Cache-Control': 'no-cache',
  DNT: '1',
  Pragma: 'no-cache',
  Referer: 'https://pleasenospam.email/',
  'sec-ch-ua': '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
  'sec-ch-ua-mobile': '?0',
  'sec-ch-ua-platform': '"Windows"',
  'sec-fetch-dest': 'empty',
  'sec-fetch-mode': 'cors',
  'sec-fetch-site': 'same-origin',
  'User-Agent':
    'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
};

/** 支持的域名列表 */
const DOMAINS = ['pleasenospam.email', 'spamlessmail.org'];

/**
 * 生成随机本地部分
 */
function randomLocalPart(length = 10): string {
  const chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
  let s = '';
  for (let i = 0; i < length; i++) {
    s += chars.charAt(Math.floor(Math.random() * chars.length));
  }
  return s;
}

/**
 * 创建 pleasenospam 临时邮箱
 * 无需调用 API，直接在本地生成随机邮箱地址
 * 支持 pleasenospam.email 和 spamlessmail.org 两个域名
 */
export async function generateEmail(domain?: string | null, channel: Channel = CHANNEL): Promise<InternalEmailInfo> {
  let dom = DOMAINS[0];
  if (domain && typeof domain === 'string') {
    const p = domain.trim().toLowerCase();
    const hit = DOMAINS.find((d) => d.toLowerCase() === p);
    if (!hit) {
      throw new Error(`pleasenospam: 不支持的域名: ${p}`);
    }
    dom = hit;
  }
  const local = randomLocalPart();
  const email = `${local}@${dom}`;
  return {
    channel,
    email,
  };
}

/**
 * pleasenospam API 返回的单封邮件结构
 * from 字段是数组，需要取 from[0] 作为发件人
 */
interface PleaseNoSpamMailItem {
  id?: string;
  from?: string[];
  subject?: string;
  receivedDate?: number;
  text?: string;
  html?: string;
}

/**
 * 获取 pleasenospam 邮件列表
 * GET /{email}.json 返回 JSON 数组
 * 空邮箱返回 []
 */
export async function getEmails(email: string): Promise<Email[]> {
  const addr = email?.trim();
  if (!addr) {
    throw new Error('pleasenospam: 邮箱地址为空');
  }
  const response = await fetchWithTimeout(`${BASE}/${encodeURIComponent(addr)}.json`, {
    method: 'GET',
    headers: DEFAULT_HEADERS,
  });
  if (!response.ok) {
    throw new Error(`pleasenospam: 获取邮件失败 HTTP ${response.status}`);
  }
  const data = (await response.json()) as unknown;
  if (!Array.isArray(data)) {
    return [];
  }
  return (data as PleaseNoSpamMailItem[]).map((item) => {
    /* from 字段是数组，取第一个元素作为发件人地址 */
    const fromAddr = Array.isArray(item.from) && item.from.length > 0 ? item.from[0] : '';
    /* receivedDate 为 Unix 时间戳（秒），需转换为毫秒 */
    let dateStr = '';
    if (typeof item.receivedDate === 'number' && item.receivedDate > 0) {
      try {
        const ts = item.receivedDate > 1e12 ? item.receivedDate : item.receivedDate * 1000;
        dateStr = new Date(ts).toISOString();
      } catch {
        /* 日期解析失败，保留空字符串 */
      }
    }
    return normalizeEmail(
      {
        id: item.id || '',
        from: fromAddr,
        to: addr,
        subject: item.subject || '',
        text: item.text || '',
        html: item.html || '',
        date: dateStr,
        isRead: false,
        attachments: [],
      },
      addr,
    );
  });
}
