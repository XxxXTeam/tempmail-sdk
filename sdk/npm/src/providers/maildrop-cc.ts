/**
 * maildrop.cc 临时邮箱渠道
 *
 * GraphQL API，无认证（公共邮箱，任何人可查看任意地址收件箱）。
 * 创建邮箱: 无需 API 调用，直接生成随机用户名 + "@maildrop.cc"。
 * 获取邮件: POST https://api.maildrop.cc/graphql
 *   - inbox(mailbox) 查询邮件列表（id/headerfrom/subject/date，无正文）
 *   - message(mailbox,id) 查询单封详情（含 html 正文）
 */
import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'maildrop-cc';
const DOMAIN = 'maildrop.cc';
const GRAPHQL_URL = 'https://api.maildrop.cc/graphql';

const DEFAULT_HEADERS: Record<string, string> = {
  Accept: 'application/json',
  'Content-Type': 'application/json',
  Origin: 'https://maildrop.cc',
  Referer: 'https://maildrop.cc/',
  'User-Agent':
    'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
};

interface MaildropCcInboxItem {
  id?: string;
  headerfrom?: string;
  subject?: string;
  date?: string;
}

interface MaildropCcMessage extends MaildropCcInboxItem {
  html?: string;
}

interface MaildropCcInboxResponse {
  data?: { inbox?: MaildropCcInboxItem[] };
}

interface MaildropCcMessageResponse {
  data?: { message?: MaildropCcMessage };
}

/** 生成随机用户名（小写字母 + 数字） */
function randomLocal(length = 10): string {
  const chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
  const bytes = new Uint8Array(length);
  crypto.getRandomValues(bytes);
  let out = '';
  for (let i = 0; i < length; i++) out += chars[bytes[i] % chars.length];
  return out;
}

/** 提取地址 @ 前的用户名部分 */
function mailboxOf(email: string): string {
  const s = email.trim();
  const at = s.indexOf('@');
  return at >= 0 ? s.slice(0, at) : s;
}

/**
 * 发送 GraphQL 请求并解析为指定类型
 * query 通过 JSON.stringify 构造 body，避免内联变量时的转义/注入问题
 */
async function doGraphQL<T>(query: string): Promise<T> {
  const response = await fetchWithTimeout(GRAPHQL_URL, {
    method: 'POST',
    headers: DEFAULT_HEADERS,
    body: JSON.stringify({ query }),
  });
  if (!response.ok) {
    throw new Error(`maildrop-cc graphql: ${response.status}`);
  }
  return (await response.json()) as T;
}

/** 构造 inbox 列表查询语句 */
function inboxQuery(mailbox: string): string {
  return `query { inbox(mailbox: ${JSON.stringify(mailbox)}) { id headerfrom subject date } }`;
}

/** 构造 message 单封详情查询语句 */
function messageQuery(mailbox: string, id: string): string {
  return `query { message(mailbox: ${JSON.stringify(mailbox)}, id: ${JSON.stringify(id)}) { id headerfrom subject date html } }`;
}

/**
 * 创建 maildrop.cc 临时邮箱
 * 无需 API 调用，直接生成随机用户名 + "@maildrop.cc"
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const email = `${randomLocal(10)}@${DOMAIN}`;
  return { channel: CHANNEL, email };
}

/**
 * 获取 maildrop.cc 邮件列表
 * 先用 inbox 查询拿到 id 列表，再并发用 message 查询逐封补全 html 正文。
 * 为公共邮箱服务，无需 token（token 参数保留以对齐接口，忽略）。
 */
export async function getEmails(_token: string, email: string): Promise<Email[]> {
  const mailbox = mailboxOf(email);
  if (!mailbox) {
    throw new Error('maildrop-cc: empty email');
  }

  /* 1. 查询邮件列表 */
  const inboxResp = await doGraphQL<MaildropCcInboxResponse>(inboxQuery(mailbox));
  const items = inboxResp.data?.inbox ?? [];
  if (items.length === 0) {
    return [];
  }

  /* 2. 并发查询每封邮件详情，补全 html 正文，失败时回退使用列表元信息 */
  const details = await Promise.all(
    items.map(async (item) => {
      const id = item.id ?? '';
      if (!id) return null;
      try {
        const msgResp = await doGraphQL<MaildropCcMessageResponse>(messageQuery(mailbox, id));
        return msgResp.data?.message ?? null;
      } catch {
        return null;
      }
    }),
  );

  /* 3. 扁平化并标准化 */
  return items.map((item, i) => {
    const detail = details[i];
    const raw = detail
      ? {
          id: detail.id ?? item.id ?? '',
          from: detail.headerfrom ?? '',
          subject: detail.subject ?? '',
          date: detail.date ?? '',
          html: detail.html ?? '',
        }
      : {
          id: item.id ?? '',
          from: item.headerfrom ?? '',
          subject: item.subject ?? '',
          date: item.date ?? '',
        };
    return normalizeEmail(raw, email);
  });
}
