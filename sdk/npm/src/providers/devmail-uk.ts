import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "devmail-uk";
const BASE_URL = "https://www.devmail.uk/api";

interface GenerateResponse {
  mailbox?: string;
  email?: string;
}

interface InboxResponse {
  mailbox?: string;
  detail?: boolean;
  count?: number;
  emails?: Array<Record<string, unknown>>;
}

function mailboxFromEmail(email: string): string {
  const address = String(email || "").trim();
  if (!address) return "";
  const index = address.indexOf("@");
  return index >= 0 ? address.slice(0, index) : address;
}

async function fetchJson(url: string): Promise<unknown> {
  const response = await fetchWithTimeout(url, {
    method: "GET",
    headers: { Accept: "application/json" },
  });
  if (!response.ok) {
    throw new Error(`devmail-uk: ${response.status}`);
  }
  return response.json();
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const data = (await fetchJson(`${BASE_URL}/new`)) as GenerateResponse;
  let email = String(data.email || "").trim();
  if (!email && String(data.mailbox || "").trim()) {
    email = `${String(data.mailbox).trim()}@devmail.uk`;
  }
  if (!email || !email.includes("@")) {
    throw new Error("devmail-uk: invalid new email response");
  }
  return { channel: CHANNEL, email };
}

export async function getEmails(email: string): Promise<Email[]> {
  const mailbox = mailboxFromEmail(email);
  if (!mailbox) throw new Error("devmail-uk: empty email");

  const data = (await fetchJson(
    `${BASE_URL}/inbox/${encodeURIComponent(mailbox)}?detail=true`,
  )) as InboxResponse;
  const rows = Array.isArray(data.emails) ? data.emails : [];
  const emails: Email[] = [];
  for (const row of rows) {
    if (!row || typeof row !== "object" || Array.isArray(row)) continue;
    emails.push(normalizeEmail(row as Record<string, unknown>, email));
  }
  return emails;
}
