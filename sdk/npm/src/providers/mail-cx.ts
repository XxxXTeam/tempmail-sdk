import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "mail-cx";
const BASE_URL = "https://mail.cx";
const DEFAULT_CLIENT_ID_PREFIX = "tempmail-sdk-";

const DEFAULT_HEADERS: Record<string, string> = {
  Accept: "application/json",
  Origin: BASE_URL,
  Referer: `${BASE_URL}/`,
  "User-Agent":
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
};

interface MailCxDomain {
  domain?: string;
  default?: boolean;
}

interface MailCxConfig {
  system_domains?: MailCxDomain[];
  ttl_seconds?: number;
}

function randomString(length: number): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  let result = "";
  for (let i = 0; i < length; i++) {
    result += chars[Math.floor(Math.random() * chars.length)];
  }
  return result;
}

function randomClientId(): string {
  return `${DEFAULT_CLIENT_ID_PREFIX}${randomString(16)}`;
}

function headers(clientId: string): Record<string, string> {
  return {
    ...DEFAULT_HEADERS,
    "X-Client-ID": clientId,
  };
}

async function getConfig(clientId: string): Promise<MailCxConfig> {
  const response = await fetchWithTimeout(`${BASE_URL}/v1/config`, {
    method: "GET",
    headers: headers(clientId),
  });

  if (!response.ok) {
    throw new Error(`mail-cx config: ${response.status}`);
  }

  return response.json();
}

function pickDomain(
  config: MailCxConfig,
  preferredDomain?: string | null,
): string {
  const domains = (config.system_domains ?? [])
    .map((d) => String(d.domain ?? "").trim())
    .filter(Boolean);
  if (domains.length === 0) {
    throw new Error("mail-cx: no system domains");
  }

  const preferred = String(preferredDomain ?? "")
    .trim()
    .replace(/^@/, "")
    .toLowerCase();
  if (preferred) {
    const exact = domains.find((d) => d.toLowerCase() === preferred);
    if (exact) return exact;
  }

  const defaultDomain = (config.system_domains ?? []).find(
    (d) => d.default && d.domain,
  )?.domain;
  if (defaultDomain) return defaultDomain;

  return domains[0]!;
}

function flattenListMessage(
  raw: Record<string, unknown>,
  recipientEmail: string,
): Record<string, unknown> {
  return {
    id: raw.id,
    from: raw.from_email || raw.from_name || "",
    to: recipientEmail,
    subject: raw.subject || "",
    text: raw.preview_text || "",
    created_at: raw.created_at,
    attachments: raw.attachments,
  };
}

async function fetchDetail(
  clientId: string,
  id: string,
): Promise<Record<string, unknown> | null> {
  const response = await fetchWithTimeout(
    `${BASE_URL}/v1/email/${encodeURIComponent(id)}`,
    {
      method: "GET",
      headers: headers(clientId),
    },
  );
  if (!response.ok) return null;
  const data = await response.json();
  return data && typeof data === "object" && !Array.isArray(data)
    ? (data as Record<string, unknown>)
    : null;
}

function flattenDetail(
  raw: Record<string, unknown>,
  recipientEmail: string,
): Record<string, unknown> {
  return {
    id: raw.id,
    from: raw.from_email || raw.from_name || "",
    to: recipientEmail,
    subject: raw.subject || "",
    text_body: raw.text_body,
    html_body: raw.html_body,
    text: raw.text_body || raw.preview_text || "",
    html: raw.html_body || "",
    created_at: raw.created_at,
    attachments: raw.attachments,
  };
}

export async function generateEmail(
  preferredDomain?: string | null,
): Promise<InternalEmailInfo> {
  const clientId = randomClientId();
  const config = await getConfig(clientId);
  const domain = pickDomain(config, preferredDomain);
  const email = `${randomString(12)}@${domain}`;
  const ttl = Number(config.ttl_seconds ?? 0);

  return {
    channel: CHANNEL,
    email,
    token: clientId,
    createdAt: new Date().toISOString(),
    expiresAt:
      ttl > 0 ? new Date(Date.now() + ttl * 1000).toISOString() : undefined,
  };
}

export async function getEmails(
  clientId: string,
  email: string,
): Promise<Email[]> {
  const response = await fetchWithTimeout(
    `${BASE_URL}/v1/inbox/${encodeURIComponent(email)}`,
    {
      method: "GET",
      headers: headers(clientId),
    },
    35000,
  );

  if (response.status === 204) {
    return [];
  }
  if (!response.ok) {
    throw new Error(`mail-cx inbox: ${response.status}`);
  }

  const data = await response.json();
  const rows = Array.isArray(data?.emails) ? data.emails : [];
  const emails = await Promise.all(
    rows.map(async (raw: unknown) => {
      if (!raw || typeof raw !== "object" || Array.isArray(raw)) {
        return normalizeEmail({}, email);
      }
      const row = raw as Record<string, unknown>;
      const id = row.id == null ? "" : String(row.id);
      if (id) {
        try {
          const detail = await fetchDetail(clientId, id);
          if (detail)
            return normalizeEmail(flattenDetail(detail, email), email);
        } catch {
          /* fall back to list row */
        }
      }
      return normalizeEmail(flattenListMessage(row, email), email);
    }),
  );

  return emails;
}
