import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "inboxes";
const API_BASE = "https://inboxes.com/api/v2";
const DEFAULT_DOMAIN = "blondmail.com";
const DOMAIN_LABEL_RE = /^[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$/i;

const HEADERS: Record<string, string> = {
  Accept: "application/json",
  Referer: "https://inboxes.com/",
  Origin: "https://inboxes.com",
  "User-Agent": "Mozilla/5.0",
};

interface DomainsResponse {
  domains?: Array<{ qdn?: string }>;
}

interface InboxResponse {
  msgs?: InboxesListMessage[];
}

interface InboxesListMessage {
  uid?: string;
  f?: string;
  s?: string;
  d?: boolean;
  at?: unknown[];
  cr?: string;
  r?: number;
  ph?: string;
  ib?: string;
}

interface InboxesDetail extends InboxesListMessage {
  ff?: Array<{ address?: string; name?: string }>;
  html?: string;
  text?: string;
  ru?: boolean;
  sf?: string;
  short_html?: string;
}

function randomLocal(): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  let out = "sdk";
  for (let i = 0; i < 16; i++)
    out += chars[Math.floor(Math.random() * chars.length)];
  return out;
}

async function fetchJSON<T>(url: string): Promise<T> {
  const response = await fetchWithTimeout(url, {
    method: "GET",
    headers: HEADERS,
  });
  if (!response.ok) {
    throw new Error(`inboxes http ${response.status}`);
  }
  return response.json() as Promise<T>;
}

function pickDomain(domains: string[], preferred?: string | null): string {
  const wanted = String(preferred || "")
    .trim()
    .replace(/^@/, "")
    .toLowerCase();
  if (wanted) {
    const hit = domains.find((d) => d.toLowerCase() === wanted);
    if (hit) return hit;
  }
  return (
    domains.find((d) => d === DEFAULT_DOMAIN) ?? domains[0] ?? DEFAULT_DOMAIN
  );
}

async function getDomains(): Promise<string[]> {
  const data = await fetchJSON<DomainsResponse>(`${API_BASE}/domain`);
  const domains = (Array.isArray(data.domains) ? data.domains : [])
    .map((item) =>
      String(item.qdn || "")
        .trim()
        .toLowerCase(),
    )
    .filter(isMailDomain);
  if (domains.length === 0) throw new Error("inboxes: no domains");
  return domains;
}

function isMailDomain(domain: string): boolean {
  if (!domain || domain.length > 253 || domain.includes("..")) return false;
  const labels = domain.split(".");
  return (
    labels.length >= 2 && labels.every((label) => DOMAIN_LABEL_RE.test(label))
  );
}

function flatten(
  raw: InboxesDetail | InboxesListMessage,
  recipient: string,
): Record<string, unknown> {
  return {
    id: raw.uid || "",
    from: (raw as InboxesDetail).sf || raw.f || "",
    to: raw.ib || recipient,
    subject: raw.s || "",
    text: (raw as InboxesDetail).text || "",
    html: (raw as InboxesDetail).html || "",
    preview_text: raw.ph || "",
    date: raw.cr || "",
    isRead: Boolean((raw as InboxesDetail).ru),
    attachments: Array.isArray(raw.at) ? raw.at : [],
  };
}

async function fetchDetail(uid: string): Promise<InboxesDetail | null> {
  try {
    return await fetchJSON<InboxesDetail>(
      `${API_BASE}/message/${encodeURIComponent(uid)}`,
    );
  } catch {
    return null;
  }
}

export async function generateEmail(
  domain?: string | null,
): Promise<InternalEmailInfo> {
  const domains = await getDomains();
  const selectedDomain = pickDomain(domains, domain);
  const email = `${randomLocal()}@${selectedDomain}`;
  return { channel: CHANNEL, email };
}

export async function getEmails(email: string): Promise<Email[]> {
  const address = String(email || "").trim();
  if (!address) throw new Error("inboxes: empty email");

  const data = await fetchJSON<InboxResponse>(
    `${API_BASE}/inbox/${encodeURIComponent(address)}`,
  );
  const rows = Array.isArray(data.msgs) ? data.msgs : [];
  const out: Email[] = [];
  for (const row of rows) {
    const uid = String(row.uid || "").trim();
    const detail = uid ? await fetchDetail(uid) : null;
    out.push(normalizeEmail(flatten(detail || row, address), address));
  }
  return out;
}
