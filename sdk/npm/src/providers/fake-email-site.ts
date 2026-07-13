import { Channel, Email, InternalEmailInfo } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "fake-email-site";
const BASE_URL = "https://api.fake-email.site";

const HEADERS: Record<string, string> = {
  Accept: "application/json",
  "Content-Type": "application/json",
  "User-Agent":
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
};

async function requestJson<T>(path: string, init?: RequestInit): Promise<T> {
  const response = await fetchWithTimeout(`${BASE_URL}${path}`, {
    ...init,
    headers: {
      ...HEADERS,
      ...(init?.headers || {}),
    },
  });
  const text = await response.text();
  if (!response.ok) {
    throw new Error(
      `fake-email-site ${path}: ${response.status} ${text.slice(0, 160)}`,
    );
  }
  if (!text) {
    return {} as T;
  }
  return JSON.parse(text) as T;
}

function flattenMessage(
  raw: InboxMessage,
  recipient: string,
): Record<string, unknown> {
  return {
    id: raw.id == null ? "" : String(raw.id),
    from_addr: raw.from_addr || "",
    to_addr: raw.to_addr || recipient,
    subject: raw.subject || "",
    body_text: raw.body_text || "",
    html: "",
    received_at: raw.received_at || "",
    isRead: false,
    attachments: [],
  };
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const data = await requestJson<CreateMailboxResponse>(
    "/api/temporary-address",
    {
      method: "POST",
      body: JSON.stringify({}),
    },
  );
  const email = (data.temp_email_addr || "").trim();
  if (!email) {
    throw new Error("fake-email-site: empty email response");
  }
  return {
    channel: CHANNEL,
    email,
    token: email,
  };
}

export async function getEmails(email: string): Promise<Email[]> {
  const address = String(email || "").trim();
  if (!address) {
    throw new Error("fake-email-site: empty email");
  }

  try {
    const data = await requestJson<InboxPollResponse>(
      `/api/inbox/poll?address=${encodeURIComponent(address)}`,
    );
    const rows = Array.isArray(data.messages) ? data.messages : [];
    return rows.map((row) =>
      normalizeEmail(flattenMessage(row, address), address),
    );
  } catch (err) {
    const message = err instanceof Error ? err.message : String(err);
    if (message.includes(": 404 ")) {
      return [];
    }
    throw err;
  }
}

/**
 * 创建临时邮箱响应
 */
type CreateMailboxResponse = {
  temp_email_addr?: string;
};

/**
 * 收件箱单条消息原始结构
 */
type InboxMessage = {
  id?: string | number;
  from_addr?: string | null;
  to_addr?: string | null;
  subject?: string | null;
  body_text?: string | null;
  received_at?: string | null;
};

/**
 * 收件箱轮询响应
 */
type InboxPollResponse = {
  messages?: InboxMessage[];
};
