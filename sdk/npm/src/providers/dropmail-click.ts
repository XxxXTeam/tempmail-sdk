import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "dropmail-click";
const BASE_URL = "https://dropmail.click";

const UA =
  "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36";

/**
 * 创建 dropmail.click 临时邮箱
 * POST /api/v1/public/mailbox → { address, created_at, expires_at }
 * 邮箱有效期 10 分钟；token = email
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const resp = await fetchWithTimeout(`${BASE_URL}/api/v1/public/mailbox`, {
    method: "POST",
    headers: { "User-Agent": UA, Accept: "application/json" },
  });
  if (!resp.ok) {
    throw new Error(`dropmail-click: 创建邮箱失败 ${resp.status}`);
  }
  const data = (await resp.json()) as { address?: string; expires_at?: string };
  if (!data.address) {
    throw new Error("dropmail-click: 无效响应，缺少 address");
  }
  return {
    channel: CHANNEL,
    email: data.address,
    token: data.address,
    expiresAt: data.expires_at,
  };
}

/**
 * 获取 dropmail.click 邮件列表
 * GET /api/v1/public/mailbox/{email} → { messages: [{id, address, from, subject, text, html}] }
 */
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  const addr = token || email;
  if (!addr) {
    throw new Error("dropmail-click: 缺少邮箱地址");
  }
  const resp = await fetchWithTimeout(
    `${BASE_URL}/api/v1/public/mailbox/${encodeURIComponent(addr)}`,
    {
      method: "GET",
      headers: { "User-Agent": UA, Accept: "application/json" },
    },
  );
  if (!resp.ok) {
    throw new Error(`dropmail-click: 获取邮件失败 ${resp.status}`);
  }
  const data = (await resp.json()) as DropmailClickResponse;
  if (!Array.isArray(data.messages)) {
    return [];
  }
  return data.messages.map((msg) =>
    normalizeEmail(
      {
        id: String(msg.id ?? ""),
        from: msg.from || "",
        to: msg.address || addr,
        subject: msg.subject || "",
        text: msg.text || "",
        html: msg.html || "",
        date: msg.received_at || msg.date || "",
        isRead: false,
        attachments: [],
      },
      addr,
    ),
  );
}

type DropmailClickMessage = {
  id?: string;
  address?: string;
  from?: string;
  subject?: string;
  text?: string;
  html?: string;
  raw?: string;
  received_at?: string;
  date?: string;
};

type DropmailClickResponse = {
  messages?: DropmailClickMessage[];
};
