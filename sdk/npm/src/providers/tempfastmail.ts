import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "tempfastmail";
const BASE_URL = "https://tempfastmail.com";

interface EmailBoxResponse {
  email?: string;
  uuid?: string;
}

function flattenMessage(
  raw: Record<string, unknown>,
  recipient: string,
): Record<string, unknown> {
  return {
    id: raw.uuid,
    from: raw.from,
    to: raw.real_to || recipient,
    subject: raw.subject,
    html: raw.html,
    date: raw.received_at,
    attachments: raw.attachments,
  };
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const response = await fetchWithTimeout(`${BASE_URL}/api/email-box`, {
    method: "POST",
    headers: {
      Accept: "application/json",
      "User-Agent": "Mozilla/5.0",
    },
  });
  if (!response.ok) {
    throw new Error(`tempfastmail create http ${response.status}`);
  }
  const data = (await response.json()) as EmailBoxResponse;
  const email = String(data.email || "").trim();
  const uuid = String(data.uuid || "").trim();
  if (!email || !email.includes("@") || !uuid) {
    throw new Error("tempfastmail: invalid create response");
  }
  return {
    channel: CHANNEL,
    email,
    token: uuid,
  };
}

export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  const uuid = String(token || "").trim();
  const address = String(email || "").trim();
  if (!uuid) throw new Error("tempfastmail: empty token");
  if (!address) throw new Error("tempfastmail: empty email");

  const listResponse = await fetchWithTimeout(
    `${BASE_URL}/api/email-box/${encodeURIComponent(uuid)}/emails`,
    {
      headers: {
        Accept: "application/json",
        "User-Agent": "Mozilla/5.0",
      },
    },
  );
  if (!listResponse.ok) {
    throw new Error(`tempfastmail list http ${listResponse.status}`);
  }
  const rows = (await listResponse.json()) as Array<Record<string, unknown>>;
  if (!Array.isArray(rows)) return [];

  const emails: Email[] = [];
  for (const row of rows) {
    const messageId = String(row.uuid || "").trim();
    let raw = row;
    if (messageId) {
      const detailResponse = await fetchWithTimeout(
        `${BASE_URL}/api/email-box/${encodeURIComponent(uuid)}/email/${encodeURIComponent(messageId)}`,
        {
          headers: {
            Accept: "application/json",
            "User-Agent": "Mozilla/5.0",
          },
        },
      );
      if (detailResponse.ok) {
        raw = (await detailResponse.json()) as Record<string, unknown>;
      }
    }
    emails.push(normalizeEmail(flattenMessage(raw, address), address));
  }
  return emails;
}
