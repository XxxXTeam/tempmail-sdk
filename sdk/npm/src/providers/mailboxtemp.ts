import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "mailboxtemp";
const API_BASE = "https://mailboxtemp.com";

async function getJson(path: string): Promise<any> {
  const response = await fetchWithTimeout(`${API_BASE}${path}`, {
    method: "GET",
    headers: { Accept: "application/json" },
  });
  const data = await response.json();
  if (!response.ok || data.ok === false) {
    throw new Error(`mailboxtemp: HTTP ${response.status}`);
  }
  return data;
}

function flattenDetail(raw: any, recipient: string): Record<string, unknown> {
  return {
    id: raw.id || "",
    from: raw.from_address || "",
    to: recipient,
    subject: raw.subject || "",
    text: raw.body_text || "",
    html: raw.body_html || "",
    date: raw.received_at || "",
    read: raw.read,
  };
}

export async function generateEmail(
  domain?: string | null,
  channel: Channel = CHANNEL,
): Promise<InternalEmailInfo> {
  const response = await fetchWithTimeout(`${API_BASE}/api/inbox/generate`, {
    method: "POST",
    headers: { Accept: "application/json" },
  });
  const data = (await response.json()) as {
    ok?: boolean;
    address?: string;
    expires?: string;
  };
  if (!response.ok || data.ok === false || !data.address) {
    throw new Error(`mailboxtemp: create inbox failed HTTP ${response.status}`);
  }
  return { channel, email: data.address, expiresAt: data.expires };
}

export async function getEmails(email: string): Promise<Email[]> {
  const address = (email || "").trim();
  if (!address) throw new Error("mailboxtemp: empty email");

  const pathAddress = encodeURIComponent(address);
  const data = await getJson(`/api/inbox/${pathAddress}/emails`);
  const list = Array.isArray(data.emails) ? data.emails : [];
  const emails: Email[] = [];
  for (const item of list) {
    const id = String(item?.id || "").trim();
    if (!id) continue;
    try {
      const detail = await getJson(
        `/api/email/${pathAddress}/${encodeURIComponent(id)}`,
      );
      if (detail.email) {
        const flat = flattenDetail(detail.email, address);
        if (Array.isArray(detail.attachments)) flat.attachments = detail.attachments;
        emails.push(normalizeEmail(flat, address));
        continue;
      }
    } catch {
      /* 详情失败时退回摘要 */
    }
    emails.push(
      normalizeEmail(
        {
          id,
          from: item.from_address || "",
          to: address,
          subject: item.subject || "",
          date: item.received_at || "",
          read: item.read,
        },
        address,
      ),
    );
  }
  return emails;
}
