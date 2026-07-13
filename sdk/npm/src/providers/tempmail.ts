import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "tempmail";
const BASE_URL = "https://api.tempmail.ing/api";

const DEFAULT_HEADERS = {
  "User-Agent":
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36",
  "Content-Type": "application/json",
  Referer: "https://tempmail.ing/",
  "sec-ch-ua":
    '"Microsoft Edge";v="143", "Chromium";v="143", "Not A(Brand";v="24"',
  "sec-ch-ua-mobile": "?0",
  "sec-ch-ua-platform": '"Windows"',
  DNT: "1",
};

export async function generateEmail(
  duration: number = 30,
): Promise<InternalEmailInfo> {
  const response = await fetchWithTimeout(`${BASE_URL}/generate`, {
    method: "POST",
    headers: DEFAULT_HEADERS,
    body: JSON.stringify({ duration }),
  });

  if (!response.ok) {
    throw new Error(`Failed to generate email: ${response.status}`);
  }

  const data = await response.json();

  if (!data.success) {
    throw new Error("Failed to generate email");
  }

  return {
    channel: CHANNEL,
    email: data.email.address,
    expiresAt: data.email.expiresAt,
    createdAt: data.email.createdAt,
  };
}

export async function getEmails(email: string): Promise<Email[]> {
  const encodedEmail = encodeURIComponent(email);
  const response = await fetchWithTimeout(
    `${BASE_URL}/emails/${encodedEmail}`,
    {
      method: "GET",
      headers: DEFAULT_HEADERS,
    },
  );

  if (!response.ok) {
    throw new Error(`Failed to get emails: ${response.status}`);
  }

  const data = await response.json();

  if (!data.success) {
    throw new Error("Failed to get emails");
  }

  const rawEmails = data.emails || [];
  return rawEmails.map((raw: any) =>
    normalizeEmail(flattenMessage(raw, email), email),
  );
}

/**
 * 将 tempmail.ing 的原始邮件格式扁平化
 *
 * API 返回格式:
 *   from_address: 发件人邮箱
 *   content: HTML 内容（非纯文本）
 *   text: 纯文本（通常为空）
 *   received_at: 接收时间
 *   is_read: 0/1
 */
function flattenMessage(raw: any, recipientEmail: string): any {
  return {
    id: raw.id ?? "",
    from: raw.from_address || raw.from || "",
    to: recipientEmail,
    subject: raw.subject || "",
    text: raw.text || "",
    html: raw.content || raw.html || "",
    date: raw.received_at || raw.date || "",
    isRead: raw.is_read === 1 || raw.is_read === true,
    attachments: raw.attachments || [],
  };
}
