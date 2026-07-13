import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "mail10s";
const BASE_URL = "https://mail10s.com";
const DEFAULT_DOMAIN = "mail10s.com";

interface InboxResponse {
  success?: boolean;
  data?: {
    messages?: Array<Record<string, unknown>>;
  };
}

function randomLocal(): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  let out = "sdk";
  for (let i = 0; i < 16; i++)
    out += chars[Math.floor(Math.random() * chars.length)];
  return out;
}

function flattenMessage(
  raw: Record<string, unknown>,
  recipient: string,
): Record<string, unknown> {
  return {
    id: raw.id,
    from: raw.sender,
    to: recipient,
    subject: raw.subject,
    text: raw.body_text,
    html: raw.body_html,
    date: raw.received_at,
    attachments: raw.attachments,
  };
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const email = `${randomLocal()}@${DEFAULT_DOMAIN}`;
  return {
    channel: CHANNEL,
    email,
    token: email,
  };
}

export async function getEmails(email: string): Promise<Email[]> {
  const address = String(email || "").trim();
  if (!address) throw new Error("mail10s: empty email");

  const response = await fetchWithTimeout(
    `${BASE_URL}/api/emails/${encodeURIComponent(address)}/inbox`,
    {
      headers: {
        Accept: "application/json",
        "User-Agent": "Mozilla/5.0",
      },
    },
  );
  if (!response.ok) {
    throw new Error(`mail10s http ${response.status}`);
  }
  const data = (await response.json()) as InboxResponse;
  const messages = Array.isArray(data.data?.messages) ? data.data.messages : [];
  return messages.map((row) =>
    normalizeEmail(flattenMessage(row, address), address),
  );
}
