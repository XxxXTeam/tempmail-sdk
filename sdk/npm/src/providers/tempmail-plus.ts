import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const DEFAULT_CHANNEL: Channel = "tempmail-plus";
const API_BASE = "https://tempmail.plus/api/mails";
const DEFAULT_DOMAIN = "mailto.plus";

const HEADERS: Record<string, string> = {
  Accept: "application/json",
  "User-Agent":
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
  Referer: "https://tempmail.plus/",
  Origin: "https://tempmail.plus",
};

function randomLocal(len: number): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  let s = "";
  for (let i = 0; i < len; i++)
    s += chars[Math.floor(Math.random() * chars.length)];
  return s;
}

export async function generateEmail(
  domain: string = DEFAULT_DOMAIN,
  channel: Channel = DEFAULT_CHANNEL,
): Promise<InternalEmailInfo> {
  const local = randomLocal(12);
  const email = `${local}@${domain}`;
  const res = await fetchWithTimeout(
    `${API_BASE}/?email=${encodeURIComponent(email)}&epin=`,
    { headers: HEADERS },
  );
  if (!res.ok) throw new Error(`tempmail-plus: generate failed: ${res.status}`);
  return { channel, email };
}

export async function getEmails(email: string): Promise<Email[]> {
  const listRes = await fetchWithTimeout(
    `${API_BASE}/?email=${encodeURIComponent(email)}&epin=`,
    { headers: HEADERS },
  );
  if (!listRes.ok)
    throw new Error(`tempmail-plus: list failed: ${listRes.status}`);
  const data = (await listRes.json()) as { mail_list?: any[] };
  const list = Array.isArray(data.mail_list) ? data.mail_list : [];

  const out: Email[] = [];
  for (const item of list) {
    let text = "",
      html = "";
    if (item.mail_id) {
      try {
        const dr = await fetchWithTimeout(
          `${API_BASE}/${item.mail_id}?email=${encodeURIComponent(email)}&epin=`,
          { headers: HEADERS },
        );
        if (dr.ok) {
          const detail = (await dr.json()) as any;
          text = detail.text || "";
          html = detail.html || "";
        }
      } catch {}
    }
    out.push(
      normalizeEmail(
        {
          id: String(item.mail_id || ""),
          from: item.from_mail || item.from_name || "",
          to: email,
          subject: item.subject || "",
          text,
          html,
          date: item.time || "",
          isRead: !item.is_new,
        },
        email,
      ),
    );
  }
  return out;
}
