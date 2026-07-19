/**
 * temp-mail.org 临时邮箱渠道
 *
 * 流程：
 *   1. POST /mailbox 创建临时邮箱，获取 JWT token
 *   2. GET /messages 获取邮件列表
 *   3. GET /messages/{_id} 获取邮件详情（含 HTML 正文）
 */
import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "temp-mail-org";
const BASE_URL = "https://web2.temp-mail.org";

const COMMON_HEADERS: Record<string, string> = {
  "User-Agent":
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
  Origin: "https://temp-mail.org",
  Referer: "https://temp-mail.org/",
};

/** POST /mailbox 响应 */
interface MailboxResponse {
  token?: string;
  mailbox?: string;
}

/** GET /messages 响应中的邮件摘要 */
interface MessageSummary {
  _id?: string;
  receivedAt?: number;
  from?: string;
  subject?: string;
  bodyPreview?: string;
  attachmentsCount?: number;
}

/** GET /messages 响应 */
interface MessagesResponse {
  mailbox?: string;
  messages?: MessageSummary[];
}

/** GET /messages/{_id} 响应 */
interface MessageDetail {
  _id?: string;
  receivedAt?: number;
  mailbox?: string;
  from?: string;
  subject?: string;
  bodyPreview?: string;
  bodyHtml?: string;
  attachmentsCount?: number;
  attachments?: unknown[];
  createdAt?: string;
}

/**
 * 创建 temp-mail.org 临时邮箱
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const response = await fetchWithTimeout(`${BASE_URL}/mailbox`, {
    method: "POST",
    headers: {
      ...COMMON_HEADERS,
      "Content-Type": "application/json",
    },
  });

  if (!response.ok) {
    throw new Error(`temp-mail-org mailbox: ${response.status}`);
  }

  const data = (await response.json()) as MailboxResponse;
  if (!data.token || !data.mailbox) {
    throw new Error("temp-mail-org: 创建邮箱失败");
  }

  return {
    channel: CHANNEL,
    email: data.mailbox,
    token: data.token,
  };
}

/**
 * 获取 temp-mail-org 邮件列表
 * 先获取邮件摘要列表，再逐封获取详情
 */
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  const authHeaders: Record<string, string> = {
    ...COMMON_HEADERS,
    Authorization: `Bearer ${token}`,
  };

  /* 获取邮件列表 */
  const listResponse = await fetchWithTimeout(`${BASE_URL}/messages`, {
    method: "GET",
    headers: authHeaders,
  });

  if (!listResponse.ok) {
    throw new Error(`temp-mail-org messages: ${listResponse.status}`);
  }

  const listData = (await listResponse.json()) as MessagesResponse;
  const messages = listData.messages ?? [];

  if (messages.length === 0) {
    return [];
  }

  /* 逐封获取邮件详情 */
  const emails: Email[] = [];
  for (const msg of messages) {
    if (!msg._id) continue;

    const detailResponse = await fetchWithTimeout(
      `${BASE_URL}/messages/${msg._id}`,
      {
        method: "GET",
        headers: authHeaders,
      },
    );

    if (!detailResponse.ok) continue;

    const detail = (await detailResponse.json()) as MessageDetail;

    /* 将 Unix 时间戳转换为 ISO 字符串 */
    let dateStr = detail.createdAt ?? "";
    if (!dateStr && detail.receivedAt) {
      dateStr = new Date(detail.receivedAt * 1000).toISOString();
    }

    emails.push(
      normalizeEmail(
        {
          from: detail.from ?? "",
          to: email,
          subject: detail.subject ?? "",
          html: detail.bodyHtml ?? "",
          text: detail.bodyPreview ?? "",
          date: dateStr,
        },
        email,
      ),
    );
  }

  return emails;
}
