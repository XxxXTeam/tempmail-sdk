import { randomBytes } from "crypto";
import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "tempgbox";
const API_URL = "https://tempgbox.net/api/proxy";
const ORIGIN = "https://tempgbox.net";

type ProxyPayload = {
  alias?: {
    id?: string | number;
    alias?: string;
    email?: string;
    base_email?: string;
    created_at?: string;
    expires_at?: string;
  };
  messages?: any[];
  detail?: string;
  error?: string;
  message?: string;
};

/**
 * 上游按 X-Device-ID 限制连续生成；每个新邮箱使用新的设备 ID，
 * 后续收件继续使用 generateEmail 返回的内部 token。
 */
function randomDeviceId(): string {
  return randomBytes(32).toString("hex");
}

function randomIP(): string {
  return Array.from(randomBytes(4), (value) => String((value % 254) + 1)).join(
    ".",
  );
}

function decodeProxyPayload(html: string): ProxyPayload {
  const match = html.match(/data-x=["']([^"']+)["']/);
  if (!match) {
    throw new Error("tempgbox: missing encoded response payload");
  }
  return JSON.parse(Buffer.from(match[1], "base64").toString("utf8"));
}

async function postProxy(
  route: "generate" | "inbox",
  deviceId: string,
  body: unknown,
): Promise<ProxyPayload> {
  const ip = randomIP();
  const response = await fetchWithTimeout(`${API_URL}?route=${route}`, {
    method: "POST",
    headers: {
      Accept: "text/html,application/json",
      "Content-Type": "application/json",
      Origin: ORIGIN,
      Referer: `${ORIGIN}/`,
      "User-Agent":
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
      "X-Device-ID": deviceId,
      "X-Forwarded-For": ip,
      "X-Real-IP": ip,
      "X-Originating-IP": ip,
    },
    body: JSON.stringify(body),
  });
  const text = await response.text();
  const payload = decodeProxyPayload(text);
  if (!response.ok) {
    const reason =
      payload.detail || payload.error || payload.message || response.statusText;
    throw new Error(`tempgbox ${route} failed: ${response.status} ${reason}`);
  }
  return payload;
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const deviceId = randomDeviceId();
  const payload = await postProxy("generate", deviceId, {
    variant: "googlemail",
  });
  const alias = payload.alias;
  const email = alias?.email || alias?.alias || "";
  if (!email) {
    throw new Error("tempgbox: missing email in generate response");
  }

  return {
    channel: CHANNEL,
    email,
    token: deviceId,
    createdAt: alias?.created_at,
    expiresAt: alias?.expires_at,
  };
}

export async function getEmails(
  deviceId: string,
  email: string,
): Promise<Email[]> {
  const payload = await postProxy("inbox", deviceId, { email });
  const messages = Array.isArray(payload.messages) ? payload.messages : [];
  return messages.map((raw: any) =>
    normalizeEmail(
      {
        ...raw,
        from: raw?.from || raw?.sender || "",
        text: raw?.text || raw?.body_text || "",
        html: raw?.html || raw?.body_html || "",
        date: raw?.date || raw?.received_at || "",
        messageId: raw?.messageId || raw?.message_id || raw?.id || "",
        attachments: raw?.attachments || raw?.attachments_info || [],
      },
      email,
    ),
  );
}
