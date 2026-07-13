import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "mailhole-de";
const BASE_URL = "https://mailhole.de";
const HEADERS: Record<string, string> = {
  "User-Agent":
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
};

/**
 * 创建 mailhole.de 临时邮箱
 * API: GET /api/random → HTML 包含随机邮箱地址
 * 邮箱地址即 token（公共邮箱模式，无需额外认证）
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const response = await fetchWithTimeout(`${BASE_URL}/api/random`, {
    method: "GET",
    headers: { ...HEADERS, Accept: "text/html" },
  });

  if (!response.ok) {
    throw new Error(`mailhole-de: 创建邮箱失败 ${response.status}`);
  }

  const html = await response.text();
  const match = html.match(/([a-z0-9.]+@mailhole\.de)/);
  if (!match) {
    throw new Error("mailhole-de: 无法从响应中解析邮箱地址");
  }

  const email = match[1];
  return {
    channel: CHANNEL,
    email,
    token: email,
  };
}

/**
 * 获取 mailhole.de 邮件列表
 * API: GET /json/{email} → JSON 数组
 * @param token - 邮箱地址
 * @param email - 邮箱地址
 */
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  const addr = token || email;
  if (!addr) {
    throw new Error("mailhole-de: 缺少邮箱地址");
  }

  const response = await fetchWithTimeout(
    `${BASE_URL}/json/${encodeURIComponent(addr)}`,
    {
      method: "GET",
      headers: { ...HEADERS, Accept: "application/json" },
    },
  );

  if (!response.ok) {
    throw new Error(`mailhole-de: 获取邮件失败 ${response.status}`);
  }

  const text = await response.text();
  if (!text || text === "[]") {
    return [];
  }

  const messages = JSON.parse(text) as RawMessage[];
  if (!Array.isArray(messages)) {
    return [];
  }

  return messages.map((msg) =>
    normalizeEmail(flattenMessage(msg, email), email),
  );
}

function flattenMessage(
  msg: RawMessage,
  recipient: string,
): Record<string, unknown> {
  return {
    id: msg.id || "",
    from: msg.sender || msg.from || "",
    to: recipient,
    subject: msg.subject || "",
    text: msg.body || msg.text || "",
    html: msg.html || msg.body || "",
    received_at: msg.date || msg.received || "",
    isRead: false,
    attachments: [],
  };
}

type RawMessage = {
  id?: string;
  sender?: string;
  from?: string;
  subject?: string;
  body?: string;
  text?: string;
  html?: string;
  date?: string;
  received?: string;
};
