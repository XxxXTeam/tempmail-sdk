import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "gonebox-email";
const API_BASE = "https://api.gonebox.email/api/v1";

interface CreateInboxResponse {
  success?: boolean;
  data?: {
    id?: string;
    address?: string;
    domain?: string;
    expiresAt?: number;
    ttl?: number;
    existing?: boolean;
  };
}

interface GoneboxMessage {
  id?: string;
  from?: string;
  sender?: string;
  to?: string;
  subject?: string;
  text?: string;
  html?: string;
  date?: string;
  receivedAt?: string;
}

interface MessagesResponse {
  success?: boolean;
  data?: {
    address?: string;
    expiresAt?: number;
    ttl?: number;
    count?: number;
    messages?: GoneboxMessage[];
  };
}

/**
 * 创建 gonebox.email 临时邮箱
 * API: POST /inboxes body: {"domain":"gonebox.email"}
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const response = await fetchWithTimeout(`${API_BASE}/inboxes`, {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
      Accept: "application/json",
    },
    body: JSON.stringify({ domain: "gonebox.email" }),
  });
  if (!response.ok) {
    throw new Error(`gonebox-email: 创建邮箱失败 ${response.status}`);
  }

  const resp = (await response.json()) as CreateInboxResponse;
  if (!resp.success || !resp.data) {
    throw new Error("gonebox-email: 创建邮箱响应无效");
  }

  const address = String(resp.data.address || "").trim();
  if (!address || !address.includes("@")) {
    throw new Error("gonebox-email: 创建邮箱响应缺少有效地址");
  }

  const expiresAt = resp.data.expiresAt;

  return {
    channel: CHANNEL,
    email: address,
    token: "",
    ...(expiresAt ? { expiresAt } : {}),
  };
}

/**
 * 获取 gonebox.email 邮件列表
 * API: GET /inboxes/{address}/messages
 * @param _token - 未使用（该渠道无需认证）
 * @param email - 邮箱地址
 */
export async function getEmails(
  _token: string,
  email: string,
): Promise<Email[]> {
  const address = String(email || "").trim();
  if (!address) {
    throw new Error("gonebox-email: 邮箱地址为空");
  }

  const response = await fetchWithTimeout(
    `${API_BASE}/inboxes/${encodeURIComponent(address)}/messages`,
    {
      method: "GET",
      headers: { Accept: "application/json" },
    },
  );
  if (!response.ok) {
    throw new Error(`gonebox-email: 获取邮件失败 ${response.status}`);
  }

  const resp = (await response.json()) as MessagesResponse;
  const messages: GoneboxMessage[] = Array.isArray(resp.data?.messages)
    ? resp.data!.messages
    : [];

  return messages.map((msg) => {
    const raw: Record<string, unknown> = {
      id: msg.id || "",
      from: msg.from || msg.sender || "",
      to: msg.to || address,
      subject: msg.subject || "",
      text: msg.text || "",
      html: msg.html || "",
      date: msg.date || msg.receivedAt || "",
    };
    return normalizeEmail(raw, address);
  });
}
