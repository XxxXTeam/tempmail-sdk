import { EmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';

const CHANNEL: Channel = 'dropmail';
const BASE_URL = 'https://dropmail.me/api/graphql/MY_TOKEN';

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

  const response = await fetch(BASE_URL, {
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
export async function generateEmail(): Promise<EmailInfo> {
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
