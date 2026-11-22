import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "dummyinbox";
const API_BASE = "https://dummyinbox.com/api/v1";

/** 按官方文档保留地址中的 @ 原样拼入路径 */
function pathAddress(email: string): string {
  return encodeURIComponent((email || "").trim()).replace(/%40/g, "@");
}

async function getJson(path: string): Promise<any> {
  const response = await fetchWithTimeout(`${API_BASE}${path}`, {
    method: "GET",
    headers: { Accept: "application/json" },
  });
  if (!response.ok) {
    throw new Error(`dummyinbox: HTTP ${response.status}`);
  }
  return response.json();
}

function flattenDetail(raw: any, recipient: string): Record<string, unknown> {
  const to = Array.isArray(raw.to) ? raw.to[0] : raw.to;
  return {
    id: raw.id || "",
    from: raw.from || "",
    to: to || recipient,
    subject: raw.subject || "",
    text: raw.text || "",
    html: raw.html || "",
    date: raw.receivedAt || "",
    attachments: raw.attachments,
  };
}

export async function generateEmail(
  domain?: string | null,
  channel: Channel = CHANNEL,
): Promise<InternalEmailInfo> {
  const response = await fetchWithTimeout(`${API_BASE}/inboxes`, {
    method: "POST",
    headers: { Accept: "application/json" },
  });
  if (!response.ok) {
    throw new Error(`dummyinbox: create inbox failed HTTP ${response.status}`);
  }
  const data = (await response.json()) as { address?: string };
  if (!data.address) throw new Error("dummyinbox: empty address");
  return { channel, email: data.address };
}

export async function getEmails(email: string): Promise<Email[]> {
  const address = (email || "").trim();
  if (!address) throw new Error("dummyinbox: empty email");

  const data = await getJson(
    `/inboxes/${pathAddress(address)}/messages?limit=50`,
  );
  const list = Array.isArray(data.messages) ? data.messages : [];
  const emails: Email[] = [];
  for (const item of list) {
    const id = String(item?.id || "").trim();
    if (!id) continue;
    try {
      const detail = await getJson(
        `/inboxes/${pathAddress(address)}/messages/${encodeURIComponent(id)}`,
      );
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
          from: item.from || "",
          to: address,
          subject: item.subject || "",
          text: item.snippet || "",
          date: item.receivedAt || "",
        },
        address,
      ),
    );
  }
  return emails;
}
