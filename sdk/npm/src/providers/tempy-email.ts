import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "tempy-email";
const API_BASE = "https://tempy.email/api/v1";

type CreateResponse = {
  email?: string;
  expires_at?: string;
  web_url?: string;
  webhook_url?: string | null;
  webhook_format?: string | null;
};

type ListResponse = {
  messages?: Array<Record<string, unknown>>;
};

function flattenMessage(
  raw: Record<string, unknown>,
  recipient: string,
): Record<string, unknown> {
  return {
    id: raw.id || raw.messageId || raw.message_id || raw.mail_id || "",
    from: raw.from || raw.sender || raw.from_addr || raw.from_address || "",
    to: raw.to || recipient,
    subject: raw.subject || raw.mail_title || "",
    text: raw.text || raw.body_text || raw.text_body || raw.body || "",
    html: raw.html || raw.body_html || raw.html_body || "",
    date: raw.date || raw.received_at || raw.created_at || "",
    isRead: raw.is_read ?? raw.isRead ?? raw.seen ?? false,
    attachments: raw.attachments || [],
  };
}

export async function generateEmail(
  domain: string | null = null,
  channel: Channel = CHANNEL,
): Promise<InternalEmailInfo> {
  const body: Record<string, unknown> = {};
  const wantedDomain = String(domain || "").trim();
  if (wantedDomain) {
    body.domain = wantedDomain;
  }

  const response = await fetchWithTimeout(`${API_BASE}/mailbox`, {
    method: "POST",
    headers: {
      Accept: "application/json",
      "Content-Type": "application/json",
      "User-Agent":
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
    },
    body: JSON.stringify(body),
  });
  if (!response.ok) {
    throw new Error(`tempy-email create failed: ${response.status}`);
  }
  const data = (await response.json()) as CreateResponse;
  const email = String(data.email || "").trim();
  if (!email) {
    throw new Error("tempy-email: invalid create response");
  }
  return {
    channel,
    email,
    expiresAt: data.expires_at,
  };
}

export async function getEmails(email: string): Promise<Email[]> {
  const address = String(email || "").trim();
  if (!address) {
    throw new Error("tempy-email: empty email");
  }

  const response = await fetchWithTimeout(
    `${API_BASE}/mailbox/${encodeURIComponent(address)}/messages`,
    {
      headers: {
        Accept: "application/json",
        "User-Agent":
          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
      },
    },
  );
  if (!response.ok) {
    throw new Error(`tempy-email messages failed: ${response.status}`);
  }
  const data = (await response.json()) as
    ListResponse | Record<string, unknown>;
  const rows = Array.isArray((data as ListResponse).messages)
    ? (data as ListResponse).messages || []
    : Array.isArray(data)
      ? (data as Record<string, unknown>[])
      : [];
  return rows.map((row: any) =>
    normalizeEmail(flattenMessage(row, address), address),
  );
}
