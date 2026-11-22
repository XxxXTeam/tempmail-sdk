import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "tsukaisutemail";
const API_BASE = "https://tsukaisutemail.com/api/v1";

async function getJson(path: string): Promise<any> {
  const response = await fetchWithTimeout(`${API_BASE}${path}`, {
    method: "GET",
    headers: { Accept: "application/json" },
  });
  if (!response.ok) {
    throw new Error(`tsukaisutemail: HTTP ${response.status}`);
  }
  return response.json();
}

function flattenDetail(raw: any, recipient: string): Record<string, unknown> {
  return {
    id: raw.id || "",
    from: raw.from_address || "",
    to: recipient,
    subject: raw.subject || "",
    text: raw.text_body || "",
    html: raw.html_body || "",
    date: raw.received_at || "",
  };
}

export async function generateEmail(
  domain?: string | null,
  channel: Channel = CHANNEL,
): Promise<InternalEmailInfo> {
  const response = await fetchWithTimeout(`${API_BASE}/address`, {
    method: "POST",
    headers: { Accept: "application/json" },
  });
  if (!response.ok) {
    throw new Error(`tsukaisutemail: create address failed HTTP ${response.status}`);
  }
  const data = (await response.json()) as { address?: string };
  if (!data.address) throw new Error("tsukaisutemail: empty address");
  return { channel, email: data.address };
}

export async function getEmails(email: string): Promise<Email[]> {
  const address = (email || "").trim();
  if (!address) throw new Error("tsukaisutemail: empty email");

  const data = await getJson(`/inbox/${encodeURIComponent(address)}`);
  const list = Array.isArray(data.messages) ? data.messages : [];
  const emails: Email[] = [];
  for (const item of list) {
    const id = String(item?.id || "").trim();
    if (!id) continue;
    try {
      const detail = await getJson(`/message/${encodeURIComponent(id)}`);
      if (detail.message) {
        emails.push(normalizeEmail(flattenDetail(detail.message, address), address));
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
          text: item.snippet || "",
          date: item.received_at || "",
        },
        address,
      ),
    );
  }
  return emails;
}
