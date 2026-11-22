import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "nextmail";
const API_BASE = "https://api.nextmail.sh";

/** 内部令牌以 | 分隔收件箱 ID 与读取令牌 */
function splitToken(token: string): { id: string; bearer: string } {
  const parts = (token || "").trim().split("|");
  if (parts.length !== 2 || !parts[0] || !parts[1]) {
    throw new Error("nextmail: malformed internal token");
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
  if (!response.ok) throw new Error(`nextmail: HTTP ${response.status}`);
  return response.json();
}

function fromEmail(value: any): string {
  if (value && typeof value === "object") return value.email || "";
  return typeof value === "string" ? value : "";
}

export async function generateEmail(
  domain?: string | null,
  channel: Channel = CHANNEL,
): Promise<InternalEmailInfo> {
  const data = await api("/temp-mail/inboxes", "", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: "{}",
  });
  if (!data.address || !data.id || !data.token) {
    throw new Error("nextmail: incomplete creation response");
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
  const basePath = `/temp-mail/inboxes/${encodeURIComponent(id)}/messages`;
  const data = await api(`${basePath}?limit=50`, bearer);
  const list = Array.isArray(data.data) ? data.data : [];
  const emails: Email[] = [];
  for (const item of list) {
    const messageId = String(item?.id || "").trim();
    if (!messageId) continue;
    try {
      const detail = await api(`${basePath}/${encodeURIComponent(messageId)}`, bearer);
      const flat: Record<string, unknown> = {
        id: detail.id || messageId,
        from: fromEmail(detail.from),
        to: detail.to || email,
        subject: detail.subject || "",
        date: detail.receivedAt || "",
        seen: detail.seen,
      };
      if (detail.message && detail.message.decodedBody) {
        flat.html = detail.message.decodedBody;
      }
      emails.push(normalizeEmail(flat, email));
      continue;
    } catch {
      /* 详情失败时退回摘要 */
    }
    emails.push(
      normalizeEmail(
        {
          id: messageId,
          from: fromEmail(item.from),
          to: item.to || email,
          subject: item.subject || "",
          text: item.snippet || "",
          date: item.receivedAt || "",
          seen: item.seen,
        },
        email,
      ),
    );
  }
  return emails;
}
