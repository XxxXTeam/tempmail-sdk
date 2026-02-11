/**
 * Maildrop 渠道实现
 * API: GraphQL endpoint https://api.maildrop.cc/graphql
 * 
 * 特点:
 * - 无需认证，公开 GraphQL API
 * - 自带反垃圾过滤
 * - 邮箱名即用户名（任意字符串@maildrop.cc）
 * - 无过期时间限制
 */

import { EmailInfo, Email, Channel } from '../types';

const CHANNEL: Channel = 'maildrop';
const GRAPHQL_URL = 'https://api.maildrop.cc/graphql';
const DOMAIN = 'maildrop.cc';

/**
 * 解码 RFC 2047 编码的邮件头（如发件人、主题）
 * 支持 Base64 (B) 和 Quoted-Printable (Q) 编码
 */
function decodeRfc2047(str: string): string {
  if (!str) return '';
  return str.replace(/=\?([^?]+)\?(B|Q)\?([^?]*)\?=/gi, (_, charset, encoding, encoded) => {
    try {
      if (encoding.toUpperCase() === 'B') {
        return Buffer.from(encoded, 'base64').toString('utf-8');
      }
      /* Quoted-Printable: _=空格，=XX=十六进制字节 */
      const decoded = encoded
        .replace(/_/g, ' ')
        .replace(/=([0-9A-Fa-f]{2})/g, (_m: string, hex: string) => String.fromCharCode(parseInt(hex, 16)));
      return decoded;
    } catch {
      return encoded;
    }
  });
}

/**
 * 从原始 MIME 邮件源码中提取纯文本正文
 * maildrop 的 data 字段返回完整 MIME 源码，需要解析出 text/plain 部分
 */
function extractTextFromMime(raw: string): string {
  if (!raw) return '';

  /* 分离邮件头和正文（双换行分隔） */
  const headerBodySplit = raw.indexOf('\r\n\r\n');
  if (headerBodySplit === -1) return raw;

  const headers = raw.substring(0, headerBodySplit);
  const body = raw.substring(headerBodySplit + 4);

  /* 检查是否为 multipart 邮件 */
  const boundaryMatch = headers.match(/boundary="?([^";\r\n]+)"?/i);
  if (boundaryMatch) {
    const boundary = boundaryMatch[1];
    const parts = body.split('--' + boundary);

    for (const part of parts) {
      /* 查找 text/plain 部分 */
      if (part.match(/Content-Type:\s*text\/plain/i)) {
        const partHeaderEnd = part.indexOf('\r\n\r\n');
        if (partHeaderEnd === -1) continue;

        const partHeaders = part.substring(0, partHeaderEnd);
        let content = part.substring(partHeaderEnd + 4).replace(/\r\n$/, '').replace(/--$/, '').trim();

        /* 处理 Content-Transfer-Encoding */
        if (partHeaders.match(/Content-Transfer-Encoding:\s*base64/i)) {
          try {
            content = Buffer.from(content.replace(/\s/g, ''), 'base64').toString('utf-8');
          } catch { /* 解码失败则保留原文 */ }
        } else if (partHeaders.match(/Content-Transfer-Encoding:\s*quoted-printable/i)) {
          content = content
            .replace(/=\r?\n/g, '')
            .replace(/=([0-9A-Fa-f]{2})/g, (_, hex: string) => String.fromCharCode(parseInt(hex, 16)));
        }

        return content.trim();
      }
    }
  }

  /* 非 multipart：检查整体编码 */
  if (headers.match(/Content-Transfer-Encoding:\s*base64/i)) {
    try {
      return Buffer.from(body.replace(/\s/g, ''), 'base64').toString('utf-8').trim();
    } catch { /* 解码失败 */ }
  }

  return body.trim();
}

/**
 * 生成随机用户名
 */
function randomUsername(length: number = 10): string {
  const chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
  let result = '';
  for (let i = 0; i < length; i++) {
    result += chars.charAt(Math.floor(Math.random() * chars.length));
  }
  return result;
}

/**
 * 发送 GraphQL 请求
 * 使用 operationName + variables 的标准 GraphQL 格式
 */
async function graphqlRequest(
  operationName: string,
  query: string,
  variables: Record<string, string> = {},
): Promise<any> {
  const response = await fetch(GRAPHQL_URL, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      'Origin': 'https://maildrop.cc',
      'Referer': 'https://maildrop.cc/',
      'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36',
    },
    body: JSON.stringify({ operationName, variables, query }),
  });

  if (!response.ok) {
    throw new Error(`Maildrop GraphQL request failed: ${response.status}`);
  }

  const data = await response.json();
  if (data.errors && data.errors.length > 0) {
    throw new Error(`Maildrop GraphQL error: ${data.errors[0].message}`);
  }

  return data.data;
}

/**
 * 创建临时邮箱
 * Maildrop 无需注册，任意用户名即可接收邮件
 */
export async function generateEmail(): Promise<EmailInfo> {
  const username = randomUsername();
  const email = `${username}@${DOMAIN}`;

  /* 验证邮箱可用：查询一次 inbox 确认 API 正常 */
  await graphqlRequest(
    'GetInbox',
    'query GetInbox($mailbox: String!) { inbox(mailbox: $mailbox) { id } }',
    { mailbox: username },
  );

  return {
    channel: CHANNEL,
    email,
    token: username,
  };
}

/**
 * 获取邮件列表
 * 先查 inbox 获取邮件 ID 列表，再逐封获取完整内容
 */
export async function getEmails(token: string, email: string): Promise<Email[]> {
  const mailbox = token || email.split('@')[0];

  /* 查询收件箱列表 */
  const inboxData = await graphqlRequest(
    'GetInbox',
    'query GetInbox($mailbox: String!) { inbox(mailbox: $mailbox) { id headerfrom subject date } }',
    { mailbox },
  );

  const inbox = inboxData?.inbox;
  if (!Array.isArray(inbox) || inbox.length === 0) {
    return [];
  }

  /* 逐封获取完整邮件内容 */
  const emails: Email[] = [];
  for (const item of inbox) {
    try {
      const msgData = await graphqlRequest(
        'GetMessage',
        'query GetMessage($mailbox: String!, $id: String!) { message(mailbox: $mailbox, id: $id) { id headerfrom subject date data html } }',
        { mailbox, id: item.id },
      );

      const msg = msgData?.message;
      if (msg) {
        emails.push({
          id: msg.id || item.id,
          from: decodeRfc2047(msg.headerfrom || item.headerfrom || ''),
          to: email,
          subject: decodeRfc2047(msg.subject || item.subject || ''),
          text: extractTextFromMime(msg.data || ''),
          html: msg.html || '',
          date: msg.date || item.date || '',
          isRead: false,
          attachments: [],
        });
      }
    } catch {
      /* 单封邮件获取失败不影响整体 */
    }
  }

  return emails;
}
