import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "tempmailc";
const BASE_URL = "https://tempmailc.com/api/v1";

interface NewEmailResponse {
  ok?: boolean;
  email?: string;
}

interface InboxResponse {
  ok?: boolean;
  email?: string;
  count?: number;
  messages?: Array<Record<string, unknown>>;
}

function flattenListMessage(
  raw: Record<string, unknown>,
  recipient: string,
): Record<string, unknown> {
  return {
    id: raw.id,
    from: raw.from,
    to: recipient,
    subject: raw.subject || "",
    timestamp: raw.ts,
    read: raw.read,
  };
}

async function fetchDetail(
  email: string,
  id: string,
): Promise<Record<string, unknown> | null> {
  const url = `${BASE_URL}/message?email=${encodeURIComponent(email)}&msg_id=${encodeURIComponent(id)}`;
  const response = await fetchWithTimeout(url, {
    method: "GET",
    headers: { Accept: "application/json" },
  });
  if (!response.ok) return null;
  const data = await response.json();
  return data && typeof data === "object" && !Array.isArray(data)
    ? (data as Record<string, unknown>)
    : null;
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const response = await fetchWithTimeout(`${BASE_URL}/new`, {
    method: "GET",
    headers: { Accept: "application/json" },
  });
  if (!response.ok) {
    throw new Error(`tempmailc generate: ${response.status}`);
  }

  const data = (await response.json()) as NewEmailResponse;
  const email = String(data.email || "").trim();
  if (!data.ok || !email || !email.includes("@")) {
    throw new Error("tempmailc: invalid new email response");
  }

  return { channel: CHANNEL, email };
}

export async function getEmails(email: string): Promise<Email[]> {
  const address = String(email || "").trim();
  if (!address) throw new Error("tempmailc: empty email");

  const response = await fetchWithTimeout(
    `${BASE_URL}/inbox?email=${encodeURIComponent(address)}`,
    {
      method: "GET",
      headers: { Accept: "application/json" },
    },
  );
  if (!response.ok) {
    throw new Error(`tempmailc inbox: ${response.status}`);
  }

  const data = (await response.json()) as InboxResponse;
  if (!data.ok) {
    throw new Error("tempmailc: inbox response failed");
  }

  const rows = Array.isArray(data.messages) ? data.messages : [];
  const emails: Email[] = [];
  for (const row of rows) {
    const id = row.id == null ? "" : String(row.id);
    if (id) {
      const detail = await fetchDetail(address, id);
      if (detail && detail.ok !== false) {
        emails.push(normalizeEmail(detail, address));
        continue;
      }
    }
    emails.push(normalizeEmail(flattenListMessage(row, address), address));
  }
  return emails;
}
