import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "tempgo-email";
const API_BASE = "https://tempgo.email";

interface CreateResponse {
  email?: string;
  expires_at?: string;
  expires_in?: number;
  mailbox_id?: string;
  token?: string;
}

interface InboxResponse {
  email?: string;
  expires_in?: number;
  messages?: Record<string, unknown>[];
}

/**
 * 创建 tempgo.email 临时邮箱
 * API: POST /api/generate（无 body，不发送 Content-Type header）
 * token 存储为 mailbox_id
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const response = await fetchWithTimeout(`${API_BASE}/api/generate`, {
    method: "POST",
    headers: { Accept: "application/json" },
  });
  if (!response.ok) {
    throw new Error(`tempgo-email: 创建邮箱失败 ${response.status}`);
  }

  const resp = (await response.json()) as CreateResponse;
  const address = String(resp.email || "").trim();
  const mailboxId = String(resp.mailbox_id || "").trim();

  if (!address || !address.includes("@")) {
    throw new Error("tempgo-email: 创建邮箱响应缺少有效地址");
  }
  if (!mailboxId) {
    throw new Error("tempgo-email: 创建邮箱响应缺少 mailbox_id");
  }

  return {
    channel: CHANNEL,
    email: address,
    token: mailboxId,
  };
}

/**
 * 获取 tempgo.email 邮件列表
 * API: GET /api/inbox?email={email}&mailbox_id={mailbox_id}
 * @param token - mailbox_id（创建邮箱时返回）
 * @param email - 邮箱地址
 */
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  if (!token) {
    throw new Error("tempgo-email: mailbox_id（token）不能为空");
  }

  const address = String(email || "").trim();
  if (!address) {
    throw new Error("tempgo-email: 邮箱地址为空");
  }

  const params = new URLSearchParams({ email: address, mailbox_id: token });
  const response = await fetchWithTimeout(
    `${API_BASE}/api/inbox?${params.toString()}`,
    {
      method: "GET",
      headers: { Accept: "application/json" },
    },
  );
  if (!response.ok) {
    throw new Error(`tempgo-email: 获取邮件失败 ${response.status}`);
  }

  const resp = (await response.json()) as InboxResponse;
  const messages = Array.isArray(resp.messages) ? resp.messages : [];

  return messages.map((msg) => {
    const raw: Record<string, unknown> = {
      id: msg.id || "",
      from: msg.from || msg.sender || msg.from_address || "",
      to: msg.to || address,
      subject: msg.subject || "",
      text: msg.text || msg.body || "",
      html: msg.html || msg.body_html || "",
      date: msg.date || msg.received_at || msg.created_at || "",
      isRead: msg.is_read || false,
    };
    return normalizeEmail(raw, address);
  });
}
