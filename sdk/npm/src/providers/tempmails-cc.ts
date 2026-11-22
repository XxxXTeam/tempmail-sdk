import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "tempmails-cc";
const API_BASE = "https://tempmails.cc";

function randomLocal(): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  let out = "";
  for (let i = 0; i < 16; i++)
    out += chars[Math.floor(Math.random() * chars.length)];
  return out;
}

async function getJson(path: string): Promise<any> {
  const response = await fetchWithTimeout(`${API_BASE}${path}`, {
    method: "GET",
    headers: { Accept: "application/json" },
  });
  if (!response.ok) throw new Error(`tempmails-cc: HTTP ${response.status}`);
  return response.json();
}

async function pickDomain(): Promise<string> {
  const data = await getJson("/api/domains");
  const list = Array.isArray(data.domainInfo) ? data.domainInfo : [];
  for (const item of list) {
    const domain = String(item?.domain || "").trim();
    if (domain) return domain;
  }
  throw new Error("tempmails-cc: no public domain");
}

export async function generateEmail(
  domain?: string | null,
  channel: Channel = CHANNEL,
): Promise<InternalEmailInfo> {
  const selected = await pickDomain();
  return { channel, email: `${randomLocal()}@${selected}` };
}

/** 跳过选域，在指定固定域名上生成地址 */
export async function generateEmailAtDomain(
  domain: string,
  channel: Channel = CHANNEL,
): Promise<InternalEmailInfo> {
  const selected = (domain || "").trim();
  if (!selected) throw new Error("tempmails-cc: empty domain");
  return { channel, email: `${randomLocal()}@${selected}` };
}

export async function getEmails(email: string): Promise<Email[]> {
  const address = (email || "").trim();
  if (!address) throw new Error("tempmails-cc: empty email");

  const data = await getJson(`/api/inbox/${encodeURIComponent(address)}`);
  const list = Array.isArray(data.messages) ? data.messages : [];
  const emails: Email[] = [];
  for (const item of list) {
    const id = String(item?.id || "").trim();
    if (!id) continue;
    try {
      const detail = await getJson(
        `/api/message/${encodeURIComponent(id)}?address=${encodeURIComponent(address)}&mark_read=false`,
      );
      const to = Array.isArray(detail.to) ? detail.to[0] : detail.to;
      emails.push(
        normalizeEmail(
          {
            id: detail.id || id,
            from: detail.from || "",
            to: to || address,
            subject: detail.subject || "",
            text: detail.textBody || "",
            html: detail.htmlBody || "",
            date: detail.date || item.date || "",
            seen: detail.isRead ?? item.isRead,
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
          from: item.from || "",
          to: address,
          subject: item.subject || "",
          date: item.date || "",
          seen: item.isRead,
        },
        address,
      ),
    );
  }
  return emails;
}
