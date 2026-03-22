import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';
import { getConfig } from '../config';

const CHANNEL: Channel = 'dropmail';

const TOKEN_GENERATE_URL = 'https://dropmail.me/api/token/generate';
const TOKEN_RENEW_URL = 'https://dropmail.me/api/token/renew';
/** 申请 1h 令牌，缓存略短于 1h，避免边界过期 */
const AUTO_TOKEN_CACHE_MS = 50 * 60 * 1000;
/** 距缓存过期前多久触发 renew（毫秒） */
const RENEW_BEFORE_EXPIRY_MS = 10 * 60 * 1000;

function cacheMsForLifetime(lifetime: string): number {
  const s = lifetime.trim().toLowerCase();
  if (s === '1h') return 50 * 60 * 1000;
  if (s === '1d') return 23 * 60 * 60 * 1000;
  if (s === '7d' || s === '30d' || s === '90d') {
    const days = parseInt(s, 10);
    return Math.max(0, days - 1) * 24 * 60 * 60 * 1000;
  }
  return AUTO_TOKEN_CACHE_MS;
}

function dropmailRenewLifetime(): string {
  const c = getConfig().dropmailRenewLifetime?.trim();
  if (c) return c;
  const e =
    typeof process !== 'undefined' && process.env?.DROPMAIL_RENEW_LIFETIME?.trim();
  return e || '1d';
}

const TOKEN_HEADERS: Record<string, string> = {
  Accept: 'application/json',
  'Content-Type': 'application/json',
  Origin: 'https://dropmail.me',
  Referer: 'https://dropmail.me/api/',
};

let cachedAfToken: { value: string; expiresAt: number } | null = null;
let tokenFetchInFlight: Promise<string> | null = null;

function explicitDropmailAuthToken(): string | undefined {
  const fromConfig = getConfig().dropmailAuthToken?.trim();
  const fromEnv =
    typeof process !== 'undefined' && process.env
      ? process.env.DROPMAIL_AUTH_TOKEN?.trim() || process.env.DROPMAIL_API_TOKEN?.trim()
      : undefined;
  return fromConfig || fromEnv;
}

function dropmailAutoTokenDisabled(): boolean {
  if (getConfig().dropmailDisableAutoToken) {
    return true;
  }
  const v = typeof process !== 'undefined' && process.env?.DROPMAIL_NO_AUTO_TOKEN?.trim().toLowerCase();
  return v === '1' || v === 'true' || v === 'yes';
}

async function fetchAfTokenFromApi(): Promise<string> {
  const response = await fetchWithTimeout(TOKEN_GENERATE_URL, {
    method: 'POST',
    headers: TOKEN_HEADERS,
    body: JSON.stringify({ type: 'af', lifetime: '1h' }),
  });

  if (!response.ok) {
    throw new Error(`DropMail token/generate HTTP ${response.status}`);
  }

  const body = (await response.json()) as { token?: string; error?: string };
  const token = typeof body.token === 'string' ? body.token.trim() : '';
  if (!token || !token.startsWith('af_')) {
    throw new Error(
      body.error || 'DropMail token/generate 未返回有效 af_ 令牌',
    );
  }
  return token;
}

async function renewAfTokenFromApi(currentToken: string, lifetime: string): Promise<string> {
  const response = await fetchWithTimeout(TOKEN_RENEW_URL, {
    method: 'POST',
    headers: TOKEN_HEADERS,
    body: JSON.stringify({ token: currentToken, lifetime }),
  });

  if (!response.ok) {
    throw new Error(`DropMail token/renew HTTP ${response.status}`);
  }

  const body = (await response.json()) as { token?: string; error?: string };
  const token = typeof body.token === 'string' ? body.token.trim() : '';
  if (!token || !token.startsWith('af_')) {
    throw new Error(body.error || 'DropMail token/renew 未返回有效 af_ 令牌');
  }
  return token;
}

/**
 * 解析 GraphQL 用的 af_ 令牌：优先配置/环境变量，否则 generate + 缓存，将过期时 renew。
 */
async function resolveDropmailAuthToken(): Promise<string> {
  const explicit = explicitDropmailAuthToken();
  if (explicit) {
    return explicit;
  }

  if (dropmailAutoTokenDisabled()) {
    throw new Error(
      'DropMail 已禁用自动令牌：请设置 DROPMAIL_AUTH_TOKEN，或 setConfig({ dropmailAuthToken: "af_..." })，见 https://dropmail.me/api/',
    );
  }

  const now = Date.now();
  if (
    cachedAfToken &&
    now < cachedAfToken.expiresAt - RENEW_BEFORE_EXPIRY_MS
  ) {
    return cachedAfToken.value;
  }

  if (tokenFetchInFlight) {
    return tokenFetchInFlight;
  }

  const renewLifetime = dropmailRenewLifetime();

  tokenFetchInFlight = (async () => {
    try {
      if (cachedAfToken?.value) {
        try {
          const renewed = await renewAfTokenFromApi(
            cachedAfToken.value,
            renewLifetime,
          );
          cachedAfToken = {
            value: renewed,
            expiresAt: Date.now() + cacheMsForLifetime(renewLifetime),
          };
          return renewed;
        } catch {
          /* 续期失败则重新申请 */
        }
      }

      const token = await fetchAfTokenFromApi();
      cachedAfToken = {
        value: token,
        expiresAt: Date.now() + AUTO_TOKEN_CACHE_MS,
      };
      return token;
    } finally {
      tokenFetchInFlight = null;
    }
  })();

  return tokenFetchInFlight;
}

async function dropmailGraphqlUrl(): Promise<string> {
  const token = await resolveDropmailAuthToken();
  return `https://dropmail.me/api/graphql/${encodeURIComponent(token)}`;
}

const DEFAULT_HEADERS: Record<string, string> = {
  'Content-Type': 'application/x-www-form-urlencoded',
};

const CREATE_SESSION_QUERY = 'mutation {introduceSession {id, expiresAt, addresses{id, address}}}';

const GET_MAILS_QUERY = `query ($id: ID!) {
  session(id:$id) {
    mails {
      id, rawSize, fromAddr, toAddr, receivedAt,
      text, headerFrom, headerSubject, html
    }
  }
}`;

/**
 * 执行 GraphQL 请求
 */
async function graphqlRequest(query: string, variables?: Record<string, any>): Promise<any> {
  const params = new URLSearchParams();
  params.set('query', query);
  if (variables) {
    params.set('variables', JSON.stringify(variables));
  }

  const response = await fetchWithTimeout(await dropmailGraphqlUrl(), {
    method: 'POST',
    headers: DEFAULT_HEADERS,
    body: params.toString(),
  });

  if (!response.ok) {
    throw new Error(`GraphQL request failed: ${response.status}`);
  }

  const data = await response.json();

  if (data.errors) {
    throw new Error(`GraphQL error: ${data.errors[0]?.message || 'Unknown error'}`);
  }

  return data.data;
}

/**
 * 创建临时邮箱
 * GraphQL mutation: introduceSession
 * 返回 session ID (存入 token) 和邮箱地址
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const data = await graphqlRequest(CREATE_SESSION_QUERY);

  const session = data?.introduceSession;
  if (!session || !session.addresses || session.addresses.length === 0) {
    throw new Error('Failed to generate email');
  }

  return {
    channel: CHANNEL,
    email: session.addresses[0].address,
    token: session.id,
    expiresAt: session.expiresAt,
  };
}

/**
 * 将 dropmail 的邮件格式扁平化为 normalizeEmail 可处理的格式
 */
function flattenMessage(mail: any, recipientEmail: string): any {
  return {
    id: mail.id || '',
    from: mail.fromAddr || '',
    to: mail.toAddr || recipientEmail,
    subject: mail.headerSubject || '',
    text: mail.text || '',
    html: mail.html || '',
    received_at: mail.receivedAt || '',
    attachments: [],
  };
}

/**
 * 获取邮件列表
 * GraphQL query: session(id) { mails {...} }
 * token 中存储的是 session ID
 */
export async function getEmails(token: string, email: string): Promise<Email[]> {
  const data = await graphqlRequest(GET_MAILS_QUERY, { id: token });

  const mails = data?.session?.mails || [];
  return mails.map((mail: any) => normalizeEmail(flattenMessage(mail, email), email));
}
