import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "harakirimail";
const API_BASE = "https://harakirimail.com/api/v1";
const DOMAIN = "harakirimail.com";

function randomLocal(len: number): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  let s = "";
  for (let i = 0; i < len; i++)
    s += chars[Math.floor(Math.random() * chars.length)];
  return s;
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const local = randomLocal(12);
  const email = `${local}@${DOMAIN}`;
  const res = await fetchWithTimeout(
    `${API_BASE}/inbox/${encodeURIComponent(local)}`,
    {
      headers: {
        Accept: "application/json",
        "User-Agent":
          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
      },
    },
  );
  if (!res.ok) throw new Error(`harakirimail: generate failed: ${res.status}`);
  return { channel: CHANNEL, email };
}

export async function getEmails(email: string): Promise<Email[]> {
  const local = email.split("@")[0];
  const res = await fetchWithTimeout(
    `${API_BASE}/inbox/${encodeURIComponent(local)}`,
    {
      headers: {
        Accept: "application/json",
        "User-Agent":
          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
      },
    },
  );
  if (!res.ok)
    throw new Error(`harakirimail: get emails failed: ${res.status}`);
  const data = (await res.json()) as { emails?: any[] };
  const list = Array.isArray(data.emails) ? data.emails : [];
  const out: Email[] = [];
  for (const item of list) {
    let body = item.body || item.text || "";
    let html = item.html || item.body_html || "";
    if (item._id && !body && !html) {
      try {
        const dr = await fetchWithTimeout(
          `${API_BASE}/email/${encodeURIComponent(item._id)}`,
          {
            headers: {
              Accept: "application/json",
              "User-Agent":
                "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
            },
          },
        );
        if (dr.ok) {
          const detail = (await dr.json()) as any;
          body = detail.body || detail.text || body;
          html = detail.html || detail.body_html || html;
        }
      } catch {}
    }
    out.push(
      normalizeEmail(
        {
          id: item._id || item.id || "",
          from: item.from || "",
          to: email,
          subject: item.subject || "",
          text:
            typeof body === "string"
              ? body
                  .replace(/<[^>]+>/g, " ")
                  .replace(/\s+/g, " ")
                  .trim()
              : "",
          html: html || body || "",
          date: item.received || item.date || "",
          isRead: false,
        },
        email,
      ),
    );
  }
  return out;
}
