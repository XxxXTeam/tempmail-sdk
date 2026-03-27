import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'boomlify';
const BASE_URL = 'https://v1.boomlify.com';

const DEFAULT_HEADERS = {
  'Accept': 'application/json, text/plain, */*',
  'Accept-Language': 'zh',
  'Origin': 'https://boomlify.com',
  'Referer': 'https://boomlify.com/',
  'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
  'sec-ch-ua': '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
  'sec-ch-ua-mobile': '?0',
  'sec-ch-ua-platform': '"Windows"',
  'sec-fetch-dest': 'empty',
  'sec-fetch-mode': 'cors',
  'sec-fetch-site': 'same-site',
  'x-user-language': 'zh',
};

interface BoomlifyDomain {
  id: string;
  domain: string;
  created_at: string;
  is_premium: number;
  is_edu: number;
  expires_at: string;
  is_active: number;
}

interface BoomlifyPublicCreateResponse {
  id?: string;
  error?: string;
  message?: string;
  captchaRequired?: boolean;
}

interface BoomlifyEmail {
  id: string;
  temp_email_id: string;
  from_email: string;
  from_name: string;
  subject: string;
  body_html: string;
  body_text: string;
  received_at: string;
  temp_email: string;
}

async function getDomains(): Promise<BoomlifyDomain[]> {
  const response = await fetchWithTimeout(`${BASE_URL}/domains/public`, {
    method: 'GET',
    headers: DEFAULT_HEADERS,
  });

  if (!response.ok) {
    throw new Error(`Failed to get domains: ${response.status}`);
  }

  const data: BoomlifyDomain[] = await response.json();
  return data.filter(d => d.is_active === 1 && d.id && d.domain);
}

/**
 * 在服务端登记公开收件箱（需 domain 的 UUID）。
 * 仅随机 local@domain 不会入库，SMTP 邮件无法与列表 API 对应。
 */
async function createPublicInbox(domainId: string): Promise<string> {
  const response = await fetchWithTimeout(`${BASE_URL}/emails/public/create`, {
    method: 'POST',
    headers: {
      ...DEFAULT_HEADERS,
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({ domainId }),
  });

  if (!response.ok) {
    throw new Error(`Failed to create boomlify inbox: ${response.status}`);
  }

  const data: BoomlifyPublicCreateResponse = await response.json();
  if (data.error) {
    const extra = data.message ? ` — ${data.message}` : '';
    if (data.captchaRequired) {
      throw new Error(`boomlify: ${data.error}${extra}（服务端限流/需验证码，请稍后重试）`);
    }
    throw new Error(`boomlify: ${data.error}${extra}`);
  }
  if (!data.id) {
    throw new Error('boomlify: public create returned no inbox id');
  }
  return data.id;
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const domains = await getDomains();

  if (domains.length === 0) {
    throw new Error('Failed to generate email: no domains available');
  }

  const pick = domains[Math.floor(Math.random() * domains.length)];
  const boxId = await createPublicInbox(pick.id);
  /* 收件地址与列表 API 一致：inboxId@域名（与官网 public create 行为一致） */
  const email = `${boxId}@${pick.domain}`;

  return {
    channel: CHANNEL,
    email,
    token: boxId,
  };
}

function boomlifyInboxPathSegment(email: string): string {
  const at = email.indexOf('@');
  const local = at > 0 ? email.slice(0, at) : email;
  /* public create 返回的 inbox id 为 UUID；列表接口路径为 /emails/public/{id}，非完整地址 */
  const uuidRe = /^[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i;
  return uuidRe.test(local) ? local : email;
}

export async function getEmails(email: string): Promise<Email[]> {
  const pathSeg = boomlifyInboxPathSegment(email);
  const response = await fetchWithTimeout(`${BASE_URL}/emails/public/${encodeURIComponent(pathSeg)}`, {
    method: 'GET',
    headers: DEFAULT_HEADERS,
  });

  if (!response.ok) {
    throw new Error(`Failed to get emails: ${response.status}`);
  }

  const data: BoomlifyEmail[] = await response.json();

  return data.map((raw) => normalizeEmail({
    id: raw.id,
    from: raw.from_email || raw.from_name || '',
    to: email,
    subject: raw.subject || '',
    text: raw.body_text || '',
    html: raw.body_html || '',
    date: raw.received_at || '',
    isRead: false,
  }, email));
}