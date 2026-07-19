import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "mailcat-ai";
const API_BASE = "https://api.mailcat.ai";

interface CreateMailboxResponse {
  data?: {
    email?: string;
    token?: string;
  };
}

interface MailcatMessage {
  id?: string;
  from?: string;
  sender?: string;
  to?: string;
  subject?: string;
  text?: string;
  html?: string;
  date?: string;
  receivedAt?: string;
  isRead?: boolean;
}

interface InboxResponse {
  data?: MailcatMessage[];
  meta?: {
    mailbox?: string;
    unread?: number;
    pagination?: {
      offset?: number;
      limit?: number;
      totalCount?: number;
      hasMore?: boolean;
    };
  };
}

/**
 * 创建 mailcat.ai 临时邮箱
 * API: POST /mailboxes（无 body）
 * 返回 token 用于后续获取邮件
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const response = await fetchWithTimeout(`${API_BASE}/mailboxes`, {
    method: "POST",
    headers: { Accept: "application/json" },
  });
  if (!response.ok) {
    throw new Error(`mailcat-ai: 创建邮箱失败 ${response.status}`);
  }

  const resp = (await response.json()) as CreateMailboxResponse;
  const address = String(resp.data?.email || "").trim();
  const token = String(resp.data?.token || "").trim();

  if (!address || !address.includes("@")) {
    throw new Error("mailcat-ai: 创建邮箱响应缺少有效地址");
  }
  if (!token) {
    throw new Error("mailcat-ai: 创建邮箱响应缺少 token");
  }

  return {
    channel: CHANNEL,
    email: address,
    token,
  };
}

/**
 * 获取 mailcat.ai 邮件列表
 * API: GET /inbox，需要 Authorization: Bearer {token}
 * @param token - Bearer token（创建邮箱时返回）
 * @param email - 邮箱地址
 */
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  if (!token) {
    throw new Error("mailcat-ai: token 不能为空");
  }

  const address = String(email || "").trim();
  if (!address) {
    throw new Error("mailcat-ai: 邮箱地址为空");
  }

  const response = await fetchWithTimeout(`${API_BASE}/inbox`, {
    method: "GET",
    headers: {
      Accept: "application/json",
      Authorization: `Bearer ${token}`,
    },
  });
  if (!response.ok) {
    throw new Error(`mailcat-ai: 获取邮件失败 ${response.status}`);
  }

  const resp = (await response.json()) as InboxResponse;
  const messages: MailcatMessage[] = Array.isArray(resp.data) ? resp.data : [];

  return messages.map((msg) => {
    const raw: Record<string, unknown> = {
      id: msg.id || "",
      from: msg.from || msg.sender || "",
      to: msg.to || address,
      subject: msg.subject || "",
      text: msg.text || "",
      html: msg.html || "",
      date: msg.date || msg.receivedAt || "",
      isRead: msg.isRead || false,
    };
    return normalizeEmail(raw, address);
  });
}
