import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "mailinator";
const BASE_URL = "https://mailinator.com";
const PUBLIC_DOMAIN = "public";

const DEFAULT_HEADERS: Record<string, string> = {
  Accept: "application/json",
  "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
  "Cache-Control": "no-cache",
  Pragma: "no-cache",
  "User-Agent":
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
};

function randomString(length: number): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  let result = "";
  for (let i = 0; i < length; i++) {
    result += chars[Math.floor(Math.random() * chars.length)];
  }
  return result;
}

function parseInboxMessages(data: unknown): any[] {
  if (Array.isArray(data)) return data;
  if (data && typeof data === "object") {
    const obj = data as Record<string, unknown>;
    if (Array.isArray(obj.msgs)) return obj.msgs;
    if (Array.isArray(obj.data)) return obj.data;
  }
  return [];
}

function toIsoTime(value: unknown): string {
  if (value === null || value === undefined || value === "") return "";
  if (typeof value === "number" && Number.isFinite(value)) {
    const millis = value > 1e12 ? value : value * 1000;
    return new Date(millis).toISOString();
  }
  const parsed = new Date(String(value));
  return Number.isNaN(parsed.getTime()) ? "" : parsed.toISOString();
}

function textFromPayload(
  data: unknown,
  key: "text/plain" | "text" | "text/html" | "html",
): string {
  if (!data || typeof data !== "object") return "";
  const value = (data as Record<string, unknown>)[key];
  return typeof value === "string"
    ? value
    : value === undefined || value === null
      ? ""
      : String(value);
}

function attachmentUrl(url: unknown): string | undefined {
  if (typeof url !== "string" || !url) return undefined;
  if (/^https?:\/\//i.test(url)) return url;
  return `${BASE_URL}${url}`;
}

async function requestJson(path: string): Promise<any | null> {
  try {
    const response = await fetchWithTimeout(path, {
      method: "GET",
      headers: DEFAULT_HEADERS,
    });

    if (!response.ok) {
      return null;
    }

    return await response.json();
  } catch {
    return null;
  }
}

async function getInboxMessages(inbox: string): Promise<any[]> {
  const data = await requestJson(
    `${BASE_URL}/api/v2/domains/${PUBLIC_DOMAIN}/inboxes/${encodeURIComponent(inbox)}`,
  );
  return parseInboxMessages(data);
}

async function getMessageText(messageId: string): Promise<any | null> {
  return requestJson(
    `${BASE_URL}/api/v2/domains/${PUBLIC_DOMAIN}/messages/${encodeURIComponent(messageId)}/text`,
  );
}

async function getMessageHtml(messageId: string): Promise<any | null> {
  return requestJson(
    `${BASE_URL}/api/v2/domains/${PUBLIC_DOMAIN}/messages/${encodeURIComponent(messageId)}/texthtml`,
  );
}

async function getMessageAttachments(messageId: string): Promise<any | null> {
  return requestJson(
    `${BASE_URL}/api/v2/domains/${PUBLIC_DOMAIN}/messages/${encodeURIComponent(messageId)}/attachments`,
  );
}

function flattenMessage(
  summary: any,
  recipientEmail: string,
  textPayload: any,
  htmlPayload: any,
  attachmentsPayload: any,
): any {
  const attachments = Array.isArray(attachmentsPayload?.attachments)
    ? attachmentsPayload.attachments.map((att: any) => ({
        filename: att?.name || att?.filename || "",
        size: att?.size ?? att?.filesize,
        contentType:
          att?.contentType ||
          att?.content_type ||
          att?.mimeType ||
          att?.mime_type,
        downloadUrl: attachmentUrl(att?.downloadUrl || att?.url),
      }))
    : [];

  // 优先读 "text" 键（/text 端点实际返回键），回退兜底 "text/plain"（防御性编程）
  const textContent = textFromPayload(textPayload, "text") || textFromPayload(textPayload, "text/plain");
  // 优先读 "html" 键，回退兜底 "text/html"
  const htmlContent = textFromPayload(htmlPayload, "html") || textFromPayload(htmlPayload, "text/html");

  return {
    id: summary?.id || "",
    from: summary?.from || summary?.origfrom || "",
    to: summary?.to || recipientEmail,
    subject: summary?.subject || "",
    text: textContent,
    html: htmlContent,
    date: toIsoTime(summary?.time),
    seen: false,
    attachments,
  };
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const local = randomString(12);
  return {
    channel: CHANNEL,
    email: `${local}@mailinator.com`,
  };
}

export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  void token;
  const inbox = (
    email.includes("@") ? email.slice(0, email.indexOf("@")) : email
  ).trim();
  if (!inbox) {
    return [];
  }

  const messages = await getInboxMessages(inbox);
  if (messages.length === 0) {
    return [];
  }

  const flatMessages = await Promise.all(
    messages.map(async (msg: any) => {
      const messageId = String(msg?.id || msg?.messageId || "");
      if (!messageId) {
        return null;
      }

      const [textPayload, htmlPayload, attachmentsPayload] = await Promise.all([
        getMessageText(messageId),
        getMessageHtml(messageId),
        getMessageAttachments(messageId),
      ]);

      return normalizeEmail(
        flattenMessage(
          msg,
          email,
          textPayload,
          htmlPayload,
          attachmentsPayload,
        ),
        email,
      );
    }),
  );

  return flatMessages.filter((item): item is Email => Boolean(item));
}
