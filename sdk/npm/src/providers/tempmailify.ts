import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "tempmailify";
const API_BASE = "https://tempmailify.com";

function fromAddress(value: any): string {
  if (value && typeof value === "object") return String(value.address || "");
  return typeof value === "string" ? value : "";
}

async function api(
  path: string,
  bearer: string,
  init: RequestInit = {},
): Promise<any> {
  const headers: Record<string, string> = {
    Accept: "application/json",
    ...(init.headers as Record<string, string>),
  };
  if (bearer) headers.Authorization = `Bearer ${bearer}`;
  const response = await fetchWithTimeout(`${API_BASE}${path}`, {
    ...init,
    headers,
  });
  if (!response.ok) throw new Error(`tempmailify: HTTP ${response.status}`);
  return response.json();
}

export async function generateEmail(
  domain?: string | null,
  channel: Channel = CHANNEL,
): Promise<InternalEmailInfo> {
  const data = await api("/api/inbox", "", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: "{}",
  });
  if (!data.address || !data.token) {
    throw new Error("tempmailify: incomplete creation response");
  }
  return {
    channel,
    email: data.address,
    token: data.token,
    createdAt: data.createdAt !== undefined ? String(data.createdAt) : undefined,
    expiresAt: data.expiresAt,
  };
}

export async function getEmails(email: string, token: string): Promise<Email[]> {
  const pathAddress = encodeURIComponent((email || "").trim());
  const basePath = `/api/inbox/${pathAddress}`;
  const data = await api(basePath, token);
  const list = Array.isArray(data.messages) ? data.messages : [];
  const emails: Email[] = [];
  for (const item of list) {
    const id = String(item?.id || "").trim();
    if (!id) continue;
    try {
      const detail = await api(
        `${basePath}/messages/${encodeURIComponent(id)}`,
        token,
      );
      emails.push(
        normalizeEmail(
          {
            id: detail.id || id,
            from: fromAddress(detail.from),
            to: detail.to || email,
            subject: detail.subject || item.subject || "",
            text: detail.text || "",
            html: detail.html || "",
            date: detail.receivedAt || item.receivedAt || "",
            seen: detail.seen ?? item.seen,
            attachments: detail.attachments,
          },
          email,
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
          from: fromAddress(item.from),
          to: email,
          subject: item.subject || "",
          text: item.preview || "",
          date: item.receivedAt || "",
          seen: item.seen,
        },
        email,
      ),
    );
  }
  return emails;
}
