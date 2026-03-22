import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'chatgpt-org-uk';
const BASE_URL = 'https://mail.chatgpt.org.uk/api';
const HOME_URL = 'https://mail.chatgpt.org.uk/';

const DEFAULT_HEADERS = {
  'User-Agent':
    'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36',
  Accept: '*/*',
  Referer: 'https://mail.chatgpt.org.uk/',
  Origin: 'https://mail.chatgpt.org.uk',
  DNT: '1',
};

/** 从首页 HTML 解析 window.__BROWSER_AUTH（服务端注入的会话 JWT，供 X-Inbox-Token） */
function extractBrowserAuthToken(html: string): string {
  const m = html.match(/__BROWSER_AUTH\s*=\s*(\{[\s\S]*?\})\s*;/);
  if (!m) {
    return '';
  }
  try {
    const o = JSON.parse(m[1]) as { token?: string };
    return typeof o.token === 'string' ? o.token : '';
  } catch {
    return '';
  }
}

function extractGmSid(response: Response): string {
  const h = response.headers as Headers & { getSetCookie?: () => string[] };
  if (typeof h.getSetCookie === 'function') {
    for (const line of h.getSetCookie()) {
      const match = line.match(/^gm_sid=([^;]+)/);
      if (match) {
        return match[1];
      }
    }
  }
  const setCookie = response.headers.get('set-cookie') || '';
  const match = setCookie.match(/gm_sid=([^;]+)/);
  return match ? match[1] : '';
}

async function fetchHomeSession(): Promise<{ gmSid: string; browserToken: string }> {
  const response = await fetchWithTimeout(HOME_URL, {
    method: 'GET',
    headers: DEFAULT_HEADERS,
  });

  if (!response.ok) {
    throw new Error(`Failed to load mail.chatgpt.org.uk: ${response.status}`);
  }

  const html = await response.text();
  const gmSid = extractGmSid(response);
  const browserToken = extractBrowserAuthToken(html);

  if (!gmSid) {
    throw new Error('Failed to extract gm_sid cookie');
  }
  if (!browserToken) {
    throw new Error('Failed to extract __BROWSER_AUTH from homepage (API now requires browser session)');
  }

  return { gmSid, browserToken };
}

async function fetchHomeSessionWithRetry(): Promise<{ gmSid: string; browserToken: string }> {
  try {
    return await fetchHomeSession();
  } catch (error: any) {
    const message = String(error?.message || error || '').toLowerCase();
    if (message.includes('401') || message.includes('429') || message.includes('extract')) {
      return await fetchHomeSession();
    }
    throw error;
  }
}

function sleepMs(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function fetchInboxToken(email: string, gmSid: string): Promise<string> {
  const response = await fetchWithTimeout(`${BASE_URL}/inbox-token`, {
    method: 'POST',
    headers: {
      ...DEFAULT_HEADERS,
      'Content-Type': 'application/json',
      Cookie: `gm_sid=${gmSid}`,
    },
    body: JSON.stringify({ email }),
  });

  if (!response.ok) {
    throw new Error(`Failed to get inbox token: ${response.status}`);
  }

  const data = await response.json();
  const token = data?.auth?.token;
  if (!token) {
    throw new Error('Failed to get inbox token');
  }

  return token;
}

async function fetchInboxTokenWithRetry(email: string, gmSid: string): Promise<string> {
  try {
    return await fetchInboxToken(email, gmSid);
  } catch (error: any) {
    const message = String(error?.message || error || '').toLowerCase();
    if (message.includes('401')) {
      const { gmSid: sid } = await fetchHomeSessionWithRetry();
      return await fetchInboxToken(email, sid);
    }
    throw error;
  }
}

/** 列表接口需同时带 Cookie gm_sid 与 x-inbox-token，否则返回 401 */
function parseChatgptPackedToken(packed: string): { gmSid: string; inbox: string } {
  const t = packed.trim();
  if (t.startsWith('{')) {
    try {
      const o = JSON.parse(t) as { gmSid?: string; inbox?: string };
      if (typeof o.gmSid === 'string' && typeof o.inbox === 'string') {
        return { gmSid: o.gmSid, inbox: o.inbox };
      }
    } catch {
      /* ignore */
    }
  }
  return { gmSid: '', inbox: packed };
}

async function fetchEmails(
  inboxToken: string,
  email: string,
  gmSid: string,
): Promise<Email[]> {
  if (!inboxToken) {
    throw new Error('internal error: token missing for chatgpt-org-uk');
  }
  if (!gmSid) {
    throw new Error('internal error: gm_sid missing for chatgpt-org-uk');
  }
  const encodedEmail = encodeURIComponent(email);
  const response = await fetchWithTimeout(`${BASE_URL}/emails?email=${encodedEmail}`, {
    method: 'GET',
    headers: {
      ...DEFAULT_HEADERS,
      Cookie: `gm_sid=${gmSid}`,
      'x-inbox-token': inboxToken,
    },
  });

  if (!response.ok) {
    throw new Error(`Failed to get emails: ${response.status}`);
  }

  const data = await response.json();

  if (!data.success) {
    throw new Error('Failed to get emails');
  }

  const rawEmails = data.data?.emails || [];
  return rawEmails.map((raw: any) => normalizeEmail(raw, email));
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  /*
   * generate-email 常返回 429；每次重试前重新拉首页以换新 gm_sid / __BROWSER_AUTH，并做指数退避。
   */
  const maxAttempts = 6;
  let lastStatus = 0;

  for (let attempt = 0; attempt < maxAttempts; attempt++) {
    const { gmSid, browserToken } = await fetchHomeSessionWithRetry();

    const response = await fetchWithTimeout(`${BASE_URL}/generate-email`, {
      method: 'GET',
      headers: {
        ...DEFAULT_HEADERS,
        Cookie: `gm_sid=${gmSid}`,
        'X-Inbox-Token': browserToken,
      },
    });

    if (response.status === 429) {
      lastStatus = 429;
      if (attempt < maxAttempts - 1) {
        const wait = Math.min(3000 * 2 ** attempt, 60_000);
        await sleepMs(wait);
        continue;
      }
      throw new Error('Failed to generate email: 429');
    }

    if (!response.ok) {
      lastStatus = response.status;
      throw new Error(`Failed to generate email: ${response.status}`);
    }

    const data = await response.json();

    if (!data.success) {
      throw new Error('Failed to generate email');
    }

    const email = data.data.email as string;
    const tokenFromAuth = data.auth?.token as string | undefined;
    const inboxJwt = tokenFromAuth || (await fetchInboxTokenWithRetry(email, gmSid));

    return {
      channel: CHANNEL,
      email,
      token: JSON.stringify({ gmSid, inbox: inboxJwt }),
    };
  }

  throw new Error(`Failed to generate email: ${lastStatus || 'unknown'}`);
}

export async function getEmails(token: string, email: string): Promise<Email[]> {
  if (!token) {
    throw new Error('internal error: token missing for chatgpt-org-uk');
  }

  let { gmSid, inbox } = parseChatgptPackedToken(token);
  if (!gmSid) {
    gmSid = (await fetchHomeSessionWithRetry()).gmSid;
  }

  try {
    return await fetchEmails(inbox, email, gmSid);
  } catch (error: any) {
    const message = String(error?.message || error || '').toLowerCase();
    if (message.includes('401')) {
      const sess = await fetchHomeSessionWithRetry();
      const newInbox = await fetchInboxTokenWithRetry(email, sess.gmSid);
      return await fetchEmails(newInbox, email, sess.gmSid);
    }
    throw error;
  }
}
