import { Channel, Email, InternalEmailInfo } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "ockito";
const BASE_URL = "https://ockito.com/web-api";

interface LoginResponse {
  access_token?: string;
  refresh_token?: string;
  user_type?: string;
  token_type?: string;
  provider?: string | null;
}

interface AccessTokenResponse {
  access_token?: string;
  token_type?: string;
}

interface EmailResponse {
  email?: string | { email?: string };
}

interface InboxItem {
  uid?: string | number;
  timestamp?: string | number;
  sender?: string;
  subject?: string;
  snippet?: string;
}

interface InboxResponse {
  email?: { email?: string } | string;
  inbox?: InboxItem[];
}

interface MailMessageObject {
  uid?: string | number;
  sender_email?: string;
  sender_name?: string;
  from_?: string;
  from?: string;
  to?: string;
  subject?: string;
  date?: string | number;
  html?: string;
  text?: string;
  SenderEmail?: string;
  SenderName?: string;
  From?: string;
  To?: string;
  Subject?: string;
  Date?: string | number;
}

interface MailMessageResponse {
  uid?: string | number;
  self?: string;
  obj?: MailMessageObject | null;
}

interface PackedToken {
  access_token: string;
  refresh_token: string;
}

async function fetchJSON<T>(
  url: string,
  init?: RequestInit,
): Promise<{ status: number; json: T }> {
  const response = await fetchWithTimeout(url, {
    ...init,
    headers: {
      Accept: "application/json",
      ...(init?.headers || {}),
    },
  });
  const text = await response.text();
  let json: T;
  try {
    json = text ? (JSON.parse(text) as T) : ({} as T);
  } catch {
    throw new Error(`ockito invalid JSON: ${url} HTTP ${response.status}`);
  }
  return { status: response.status, json };
}

function encodeToken(accessToken: string, refreshToken: string): string {
  return JSON.stringify({
    access_token: accessToken,
    refresh_token: refreshToken,
  });
}

function decodeToken(token: string): PackedToken {
  const value = String(token || "").trim();
  if (!value) {
    throw new Error("ockito: invalid session token");
  }
  try {
    const parsed = JSON.parse(value) as Partial<PackedToken>;
    const accessToken = String(parsed.access_token || "").trim();
    const refreshToken = String(parsed.refresh_token || "").trim();
    if (!accessToken || !refreshToken) {
      throw new Error("ockito: invalid session token");
    }
    return { access_token: accessToken, refresh_token: refreshToken };
  } catch {
    throw new Error("ockito: invalid session token");
  }
}

async function refreshAccessToken(refreshToken: string): Promise<string> {
  const response = await fetchJSON<AccessTokenResponse>(
    `${BASE_URL}/grefresh`,
    {
      method: "POST",
      headers: {
        Authorization: `Bearer ${refreshToken}`,
        "X-PASSTHROUGH": "Y",
      },
    },
  );
  if (response.status < 200 || response.status >= 300) {
    throw new Error(`ockito grefresh http ${response.status}`);
  }
  const accessToken = String(response.json?.access_token || "").trim();
  if (!accessToken) {
    throw new Error("ockito: invalid refresh response");
  }
  return accessToken;
}

async function fetchBearerJSON<T>(
  url: string,
  accessTokenState: { accessToken: string },
  refreshToken: string,
  init?: RequestInit,
): Promise<T> {
  const request = async (
    accessToken: string,
  ): Promise<{ status: number; json: T }> =>
    fetchJSON<T>(url, {
      ...init,
      headers: {
        ...(init?.headers || {}),
        Authorization: `Bearer ${accessToken}`,
      },
    });

  let response = await request(accessTokenState.accessToken);
  if (response.status === 401) {
    accessTokenState.accessToken = await refreshAccessToken(refreshToken);
    response = await request(accessTokenState.accessToken);
  }
  if (response.status < 200 || response.status >= 300) {
    throw new Error(`ockito http ${response.status}`);
  }
  return response.json;
}

function normalizeEmailAddress(value: unknown): string {
  if (typeof value === "string") {
    return value.trim();
  }
  if (value && typeof value === "object") {
    const nested = value as { email?: unknown };
    return typeof nested.email === "string" ? nested.email.trim() : "";
  }
  return "";
}

function flattenInboxRow(
  raw: Record<string, unknown>,
  recipient: string,
): Record<string, unknown> {
  return {
    id: raw.uid || raw.id || "",
    from: raw.sender || raw.sender_email || raw.from || "",
    to: recipient,
    subject: raw.subject || "",
    text: raw.snippet || raw.text || "",
    html: raw.html || "",
    date: raw.timestamp || raw.received_at || raw.date || "",
    attachments: [],
  };
}

function flattenMessage(
  raw: MailMessageResponse | Record<string, unknown>,
  recipient: string,
): Record<string, unknown> {
  const obj =
    (raw as MailMessageResponse)?.obj &&
    typeof (raw as MailMessageResponse).obj === "object"
      ? ((raw as MailMessageResponse).obj as MailMessageObject)
      : (raw as Record<string, unknown>);
  return {
    id: (raw as MailMessageResponse)?.uid || obj.uid || "",
    from:
      obj.sender_email ||
      obj.SenderEmail ||
      obj.from_ ||
      obj.From ||
      obj.from ||
      obj.sender_name ||
      obj.SenderName ||
      "",
    to: obj.to || obj.To || recipient,
    subject: obj.subject || obj.Subject || "",
    text: obj.text || "",
    html: obj.html || "",
    date: obj.date || obj.Date || "",
    attachments: [],
  };
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const login = await fetchJSON<LoginResponse>(`${BASE_URL}/gtoken`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: "{}",
  });
  if (login.status < 200 || login.status >= 300) {
    throw new Error(`ockito gtoken http ${login.status}`);
  }
  const accessToken = String(login.json?.access_token || "").trim();
  const refreshToken = String(login.json?.refresh_token || "").trim();
  if (!accessToken || !refreshToken) {
    throw new Error("ockito: invalid token response");
  }

  const emailResponse = await fetchJSON<EmailResponse>(`${BASE_URL}/email`, {
    headers: {
      Authorization: `Bearer ${accessToken}`,
    },
  });
  if (emailResponse.status < 200 || emailResponse.status >= 300) {
    throw new Error(`ockito email http ${emailResponse.status}`);
  }
  const email = normalizeEmailAddress(emailResponse.json?.email);
  if (!email || !email.includes("@")) {
    throw new Error("ockito: invalid email response");
  }

  return {
    channel: CHANNEL,
    email,
    token: encodeToken(accessToken, refreshToken),
  };
}

export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  const packed = decodeToken(token);
  const accessTokenState = { accessToken: packed.access_token };
  const address = String(email || "").trim();
  if (!address) {
    throw new Error("ockito: empty email");
  }

  const inboxResponse = await fetchBearerJSON<InboxResponse>(
    `${BASE_URL}/retrieve/${encodeURIComponent(address)}/emails`,
    accessTokenState,
    packed.refresh_token,
  );
  const rows = Array.isArray(inboxResponse?.inbox) ? inboxResponse.inbox : [];
  const out: Email[] = [];

  for (const row of rows) {
    const uid = String(row?.uid || row?.timestamp || "").trim();
    if (!uid) {
      out.push(
        normalizeEmail(
          flattenInboxRow(row as Record<string, unknown>, address),
          address,
        ),
      );
      continue;
    }

    try {
      const detail = await fetchBearerJSON<MailMessageResponse>(
        `${BASE_URL}/retrieve/${encodeURIComponent(address)}/${encodeURIComponent(uid)}`,
        accessTokenState,
        packed.refresh_token,
      );
      out.push(normalizeEmail(flattenMessage(detail, address), address));
    } catch {
      out.push(
        normalizeEmail(
          flattenInboxRow(row as Record<string, unknown>, address),
          address,
        ),
      );
    }
  }

  return out;
}
