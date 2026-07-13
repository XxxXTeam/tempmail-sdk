import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "mail123";
const API_BASE = "https://mail123.fr/api/v1";

interface MailboxResponse {
  address?: string;
  box?: string;
  domain?: string;
  expires_in_days?: number;
}

interface ListResponse {
  messages?: Array<Record<string, unknown>>;
}

interface DetailResponse {
  message?: Record<string, unknown>;
}

async function fetchJSON<T>(url: string): Promise<T> {
  const response = await fetchWithTimeout(url, {
    headers: {
      Accept: "application/json",
      "User-Agent": "Mozilla/5.0",
    },
  });
  if (!response.ok) {
    throw new Error(`mail123 http ${response.status}`);
  }
  return response.json() as Promise<T>;
}

function flattenMessage(
  raw: Record<string, unknown>,
  recipient: string,
): Record<string, unknown> {
  return {
    ...raw,
    id: raw.id,
    from: raw.from,
    to: raw.to || recipient,
    subject: raw.subject,
    text: raw.text || raw.preview,
    html: raw.html,
    date: raw.date,
    isRead: typeof raw.is_unread === "boolean" ? !raw.is_unread : raw.isRead,
    attachments: raw.attachments,
  };
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const data = await fetchJSON<MailboxResponse>(`${API_BASE}/mailbox/new`);
  const email = String(data.address || "").trim();
  if (!email || !email.includes("@")) {
    throw new Error("mail123: invalid mailbox response");
  }
  return {
    channel: CHANNEL,
    email,
    token: email,
    expiresAt:
      typeof data.expires_in_days === "number"
        ? Date.now() + data.expires_in_days * 86400000
        : undefined,
  };
}

async function fetchDetail(
  address: string,
  messageId: string,
): Promise<Record<string, unknown> | null> {
  try {
    const data = await fetchJSON<DetailResponse>(
      `${API_BASE}/mailbox/${encodeURIComponent(address)}/messages/${encodeURIComponent(messageId)}`,
    );
    return data.message && typeof data.message === "object"
      ? data.message
      : null;
  } catch {
    return null;
  }
}

export async function getEmails(email: string): Promise<Email[]> {
  const address = String(email || "").trim();
  if (!address) throw new Error("mail123: empty email");

  const data = await fetchJSON<ListResponse>(
    `${API_BASE}/mailbox/${encodeURIComponent(address)}/messages?limit=50`,
  );
  const rows = Array.isArray(data.messages) ? data.messages : [];
  const emails: Email[] = [];
  for (const row of rows) {
    const id = row.id == null ? "" : String(row.id);
    const detail = id ? await fetchDetail(address, id) : null;
    emails.push(
      normalizeEmail(flattenMessage(detail || row, address), address),
    );
  }
  return emails;
}
