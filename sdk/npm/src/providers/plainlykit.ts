import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "plainlykit";
const API_BASE = "https://plainlykit.com/tools/temporary-email/api";

/** 内部令牌以 | 分隔邮箱 ID 与读取令牌 */
function splitToken(token: string): { id: string; bearer: string } {
  const parts = (token || "").trim().split("|");
  if (parts.length !== 2 || !parts[0] || !parts[1]) {
    throw new Error("plainlykit: malformed internal token");
  }
  return { id: parts[0], bearer: parts[1] };
}

async function api(
  path: string,
  bearer = "",
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
  if (!response.ok) throw new Error(`plainlykit: HTTP ${response.status}`);
  return response.json();
}

function flattenDetail(raw: any, recipient: string): Record<string, unknown> {
  return {
    id: raw.id || "",
    from: raw.sender || "",
    to: recipient,
    subject: raw.subject || "",
    text: raw.text || "",
    html: raw.html || "",
    date: raw.receivedAt || "",
  };
}

export async function generateEmail(
  domain?: string | null,
  channel: Channel = CHANNEL,
): Promise<InternalEmailInfo> {
  const data = await api("/mailboxes", "", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: "{}",
  });
  if (!data.address || !data.id || !data.token) {
    throw new Error("plainlykit: incomplete creation response");
  }
  return {
    channel,
    email: data.address,
    token: `${data.id}|${data.token}`,
    createdAt: data.createdAt ? String(data.createdAt) : undefined,
    expiresAt: data.expiresAt,
  };
}

export async function getEmails(email: string, token: string): Promise<Email[]> {
  const { id, bearer } = splitToken(token);
  const basePath = `/mailboxes/${encodeURIComponent(id)}/messages`;
  const data = await api(basePath, bearer);
  const list = Array.isArray(data.messages) ? data.messages : [];
  const emails: Email[] = [];
  for (const item of list) {
    const messageId = String(item?.id || "").trim();
    if (!messageId) continue;
    try {
      const detail = await api(
        `${basePath}/${encodeURIComponent(messageId)}`,
        bearer,
      );
      const raw = detail.message || detail;
      const flat = flattenDetail(raw, email);
      if (!flat.subject) flat.subject = item.subject || "";
      if (!flat.from) flat.from = item.sender || "";
      emails.push(normalizeEmail(flat, email));
      continue;
    } catch {
      /* 详情失败时退回摘要 */
    }
    emails.push(
      normalizeEmail(
        {
          id: messageId,
          from: item.sender || "",
          to: email,
          subject: item.subject || "",
          text: item.preview || "",
          date: item.receivedAt || "",
        },
        email,
      ),
    );
  }
  return emails;
}
