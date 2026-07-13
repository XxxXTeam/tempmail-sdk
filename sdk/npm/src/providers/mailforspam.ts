import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "mailforspam";
const API_BASE = "https://api.mailforspam.com/api";
const DEFAULT_DOMAIN = "mailforspam.com";
const DOMAINS = ["mailforspam.com", "tempmail.io", "disposable.email"];

const HEADERS: Record<string, string> = {
  Accept: "application/json",
  Referer: "https://mailforspam.com/",
  Origin: "https://mailforspam.com",
  "User-Agent":
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
};

interface MailforspamListResponse {
  emails?: MailforspamListMessage[];
}

interface MailforspamListMessage {
  id?: string;
  sender?: string;
  receiver?: string;
  subject?: string;
  date?: string;
  readAt?: string | null;
  attachments?: MailforspamAttachment[];
}

interface MailforspamDetail extends MailforspamListMessage {
  body_text?: string;
  body_html?: string;
}

interface MailforspamAttachment {
  filename?: string;
  size?: number;
  content_type?: string;
}

function mailboxEmailsUrl(email: string, pageSize = 100): string {
  return `${API_BASE}/mailboxes/${encodeURIComponent(email)}/emails?page=1&page_size=${pageSize}&sort_by=date&sort_order=desc`;
}

function randomLocal(): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  let out = "sdk";
  for (let i = 0; i < 16; i++)
    out += chars[Math.floor(Math.random() * chars.length)];
  return out;
}

function pickDomain(preferred?: string | null): string {
  const p = (preferred || "").trim().replace(/^@/, "").toLowerCase();
  if (p) {
    const hit = DOMAINS.find((d) => d.toLowerCase() === p);
    if (hit) return hit;
  }
  return DEFAULT_DOMAIN;
}

function mapAttachments(
  raw?: MailforspamAttachment[],
): Record<string, unknown>[] {
  return Array.isArray(raw)
    ? raw.map((item) => ({
        filename: item.filename || "",
        size: item.size,
        contentType: item.content_type,
      }))
    : [];
}

function flattenDetail(
  raw: MailforspamDetail,
  recipient: string,
): Record<string, unknown> {
  return {
    id: raw.id || "",
    from: raw.sender || "",
    to: raw.receiver || recipient,
    subject: raw.subject || "",
    text: raw.body_text || "",
    html: raw.body_html || "",
    date: raw.date || "",
    isRead: Boolean(raw.readAt),
    attachments: mapAttachments(raw.attachments),
  };
}

function flattenList(
  raw: MailforspamListMessage,
  recipient: string,
): Record<string, unknown> {
  return {
    id: raw.id || "",
    from: raw.sender || "",
    to: raw.receiver || recipient,
    subject: raw.subject || "",
    date: raw.date || "",
    isRead: Boolean(raw.readAt),
    attachments: mapAttachments(raw.attachments),
  };
}

async function fetchMessage(
  id: string,
  email: string,
): Promise<MailforspamDetail | null> {
  const url = `${API_BASE}/mailboxes/${encodeURIComponent(email)}/emails/${encodeURIComponent(id)}`;
  const response = await fetchWithTimeout(url, {
    method: "GET",
    headers: HEADERS,
  });
  if (!response.ok) return null;
  const data = (await response.json()) as MailforspamDetail;
  return data && typeof data === "object" ? data : null;
}

export async function generateEmail(
  domain?: string | null,
  channel: Channel = CHANNEL,
): Promise<InternalEmailInfo> {
  const selectedDomain = pickDomain(domain);
  const email = `${randomLocal()}@${selectedDomain}`;
  const response = await fetchWithTimeout(mailboxEmailsUrl(email, 1), {
    method: "GET",
    headers: HEADERS,
  });
  if (!response.ok) {
    throw new Error(
      `mailforspam: validate mailbox failed HTTP ${response.status}`,
    );
  }

  return { channel, email };
}

export async function getEmails(email: string): Promise<Email[]> {
  const address = (email || "").trim();
  if (!address) throw new Error("mailforspam: empty email");

  const response = await fetchWithTimeout(mailboxEmailsUrl(address, 100), {
    method: "GET",
    headers: HEADERS,
  });
  if (!response.ok) {
    throw new Error(`mailforspam: list mailbox failed HTTP ${response.status}`);
  }

  const data = (await response.json()) as MailforspamListResponse;
  const list = Array.isArray(data.emails) ? data.emails : [];
  const emails: Email[] = [];
  for (const item of list) {
    if (!item.id) continue;
    const detail = await fetchMessage(String(item.id), address);
    emails.push(
      normalizeEmail(
        detail ? flattenDetail(detail, address) : flattenList(item, address),
        address,
      ),
    );
  }
  return emails;
}
