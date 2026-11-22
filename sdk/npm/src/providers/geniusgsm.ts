import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "geniusgsm";
const API_BASE = "https://tempmail.geniusgsm.com/api";

function rowsOf(data: any): any[] {
  if (Array.isArray(data)) return data;
  if (data && Array.isArray(data.results)) return data.results;
  return [];
}

function fromValue(raw: any): string {
  const name = String(raw.from_name || "");
  const addr = String(raw.from_email || "");
  return name && addr ? `${name} <${addr}>` : addr;
}

async function api(path: string, init: RequestInit = {}): Promise<any> {
  const response = await fetchWithTimeout(`${API_BASE}${path}`, {
    ...init,
    headers: {
      Accept: "application/json",
      "Content-Type": "application/json",
      ...(init.headers as Record<string, string>),
    },
  });
  const data = await response.json();
  if (!response.ok) {
    throw new Error(`geniusgsm: HTTP ${response.status}`);
  }
  return data;
}

export async function generateEmail(
  domain?: string | null,
  channel: Channel = CHANNEL,
): Promise<InternalEmailInfo> {
  const domains = await api("/domains/");
  const active = rowsOf(domains).find((item) => item && item.is_active);
  if (!active) throw new Error("geniusgsm: no active domain");

  const inbox = await api("/inboxes/create_inbox/", {
    method: "POST",
    body: JSON.stringify({ domain_id: active.id, ttl: 3600 }),
  });
  if (!inbox.id || !inbox.email) {
    throw new Error("geniusgsm: incomplete creation response");
  }
  return {
    channel,
    email: inbox.email,
    token: String(inbox.id),
    expiresAt: inbox.expires_at,
    createdAt: inbox.created_at ? String(inbox.created_at) : undefined,
  };
}

export async function getEmails(email: string, token: string): Promise<Email[]> {
  if (!token) throw new Error("geniusgsm: empty inbox id");
  const data = await api(`/inboxes/${encodeURIComponent(token)}/emails/`);
  return rowsOf(data).map((item) =>
    normalizeEmail(
      {
        id: item.id || "",
        from: fromValue(item),
        to: item.inbox_email || email,
        subject: item.subject || "",
        text: item.body_text || "",
        html: item.body_html || "",
        date: item.received_at || "",
        read: item.is_read,
        attachments: item.attachments,
      },
      email,
    ),
  );
}
