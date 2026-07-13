import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "tempmailpro";
const API_BASE = "https://tempmailpro.us/api/v1";

interface MailboxResponse {
  data?: {
    token?: string;
    address?: string;
    expires_at?: number;
    created_at?: number;
  };
}

interface ListResponse {
  data?: Array<Record<string, unknown>>;
}

interface DetailResponse {
  data?: Record<string, unknown>;
}

async function fetchJSON<T>(url: string, init?: RequestInit): Promise<T> {
  const response = await fetchWithTimeout(url, {
    ...init,
    headers: {
      Accept: "application/json",
      ...(init?.headers || {}),
    },
  });
  if (!response.ok) {
    throw new Error(`tempmailpro http ${response.status}`);
  }
  return response.json() as Promise<T>;
}

function flattenMessage(
  raw: Record<string, unknown>,
  recipient: string,
): Record<string, unknown> {
  return {
    id: raw.id || raw.message_id,
    from: raw.from_address || raw.from_name,
    to: recipient,
    subject: raw.subject,
    text: raw.body_text,
    html: raw.body_html,
    date: raw.received_at,
    attachments: raw.attachments,
  };
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const data = await fetchJSON<MailboxResponse>(`${API_BASE}/mailbox/create`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({}),
  });
  const box = data.data || {};
  const email = String(box.address || "").trim();
  const token = String(box.token || "").trim();
  if (!email || !email.includes("@") || !token) {
    throw new Error("tempmailpro: invalid mailbox response");
  }
  return {
    channel: CHANNEL,
    email,
    token,
    expiresAt: box.expires_at,
    createdAt: box.created_at == null ? undefined : String(box.created_at),
  };
}

async function fetchDetail(
  token: string,
  messageId: string,
): Promise<Record<string, unknown> | null> {
  try {
    const data = await fetchJSON<DetailResponse>(
      `${API_BASE}/mailbox/${encodeURIComponent(token)}/emails/${encodeURIComponent(messageId)}`,
    );
    return data.data && typeof data.data === "object" ? data.data : null;
  } catch {
    return null;
  }
}

export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  const mailboxToken = String(token || "").trim();
  const address = String(email || "").trim();
  if (!mailboxToken) throw new Error("tempmailpro: empty token");
  if (!address) throw new Error("tempmailpro: empty email");

  const data = await fetchJSON<ListResponse>(
    `${API_BASE}/mailbox/${encodeURIComponent(mailboxToken)}/emails`,
  );
  const rows = Array.isArray(data.data) ? data.data : [];
  const emails: Email[] = [];
  for (const row of rows) {
    const id = row.id == null ? "" : String(row.id).trim();
    const detail = id ? await fetchDetail(mailboxToken, id) : null;
    emails.push(
      normalizeEmail(flattenMessage(detail || row, address), address),
    );
  }
  return emails;
}
