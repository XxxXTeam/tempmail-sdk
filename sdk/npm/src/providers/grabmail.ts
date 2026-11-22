import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "grabmail";
const API_BASE = "https://grabmail.io/api/v1";
const DOMAIN = "grabmail.io";

function randomLocal(): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  let out = "sdk";
  for (let i = 0; i < 16; i++)
    out += chars[Math.floor(Math.random() * chars.length)];
  return out;
}

async function getJson(path: string): Promise<any> {
  const response = await fetchWithTimeout(`${API_BASE}${path}`, {
    method: "GET",
    headers: { Accept: "application/json" },
  });
  if (!response.ok) throw new Error(`grabmail: HTTP ${response.status}`);
  return response.json();
}

function fromValue(raw: any): string {
  const name = typeof raw.from_name === "string" ? raw.from_name : "";
  return name ? `${name} <${raw.from || ""}>` : String(raw.from || "");
}

export async function generateEmail(
  domain?: string | null,
  channel: Channel = CHANNEL,
): Promise<InternalEmailInfo> {
  return { channel, email: `${randomLocal()}@${DOMAIN}` };
}

export async function getEmails(email: string): Promise<Email[]> {
  const address = (email || "").trim();
  if (!address) throw new Error("grabmail: empty email");

  const data = await getJson(`/mailbox?address=${encodeURIComponent(address)}`);
  const list = Array.isArray(data.messages) ? data.messages : [];
  const emails: Email[] = [];
  for (const item of list) {
    const id = String(item?.id || "").trim();
    if (!id) continue;
    try {
      const detail = await getJson(
        `/message/${encodeURIComponent(id)}?mailbox=${encodeURIComponent(address)}`,
      );
      emails.push(
        normalizeEmail(
          {
            id: detail.id || id,
            from: fromValue(detail),
            to: address,
            subject: detail.subject || "",
            text: detail.text || "",
            html: detail.html || "",
            date: detail.date || "",
            seen: detail.seen,
            attachments: detail.attachments,
          },
          address,
        ),
      );
      continue;
    } catch {
      /* 详情失败时退回摘要 */
    }
    emails.push(
      normalizeEmail(
        {
          id,
          from: fromValue(item),
          to: address,
          subject: item.subject || "",
          text: item.preview || "",
          date: item.date || "",
          seen: item.seen,
        },
        address,
      ),
    );
  }
  return emails;
}
