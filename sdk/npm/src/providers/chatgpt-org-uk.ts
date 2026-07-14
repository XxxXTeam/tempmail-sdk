import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";
import { randomInt } from "crypto";

const CHANNEL: Channel = "chatgpt-org-uk";
const BASE_URL = "https://mail.chatgpt.org.uk/api";

const DEFAULT_HEADERS: Record<string, string> = {
  "User-Agent":
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/150.0.0.0 Safari/537.36 Edg/150.0.0.0",
  Accept: "*/*",
  Referer: "https://mail.chatgpt.org.uk/zh/",
  Origin: "https://mail.chatgpt.org.uk",
  DNT: "1",
  "sec-fetch-dest": "empty",
  "sec-fetch-mode": "cors",
  "sec-fetch-site": "same-origin",
};

function randomUsername(length: number = 10): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  let out = "";
  for (let i = 0; i < length; i++) {
    out += chars[randomInt(chars.length)];
  }
  return out;
}

function extractGmSid(response: Response): string {
  const h = response.headers as Headers & { getSetCookie?: () => string[] };
  if (typeof h.getSetCookie === "function") {
    for (const line of h.getSetCookie()) {
      const match = line.match(/^gm_sid=([^;]+)/);
      if (match) return match[1];
    }
  }
  const setCookie = response.headers.get("set-cookie") || "";
  const match = setCookie.match(/gm_sid=([^;]+)/);
  return match ? match[1] : "";
}

/**
 * 获取可用域名列表
 */
async function fetchDomains(): Promise<string[]> {
  const response = await fetchWithTimeout(`${BASE_URL}/domains/public`, {
    method: "GET",
    headers: DEFAULT_HEADERS,
  });
  if (!response.ok) {
    throw new Error(`chatgpt-org-uk: 获取域名失败 ${response.status}`);
  }
  const data = (await response.json()) as {
    success: boolean;
    data: { domains: Array<{ domain_name: string; is_active: number }> };
  };
  if (!data.success || !Array.isArray(data.data?.domains)) {
    throw new Error("chatgpt-org-uk: 域名响应格式无效");
  }
  return data.data.domains
    .filter((d) => d.is_active === 1)
    .map((d) => d.domain_name);
}

/**
 * 创建收件箱并获取 inbox token + gm_sid cookie
 */
async function createInbox(email: string): Promise<{ token: string; gmSid: string }> {
  const response = await fetchWithTimeout(`${BASE_URL}/inbox-token`, {
    method: "POST",
    headers: {
      ...DEFAULT_HEADERS,
      "Content-Type": "application/json",
    },
    body: JSON.stringify({ email }),
  });
  if (!response.ok) {
    throw new Error(`chatgpt-org-uk: 创建收件箱失败 ${response.status}`);
  }
  const gmSid = extractGmSid(response);
  const data = (await response.json()) as {
    success: boolean;
    auth?: { token?: string };
  };
  if (!data.success || !data.auth?.token) {
    throw new Error("chatgpt-org-uk: inbox-token 响应缺少 token");
  }
  return { token: data.auth.token, gmSid };
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const domains = await fetchDomains();
  if (domains.length === 0) {
    throw new Error("chatgpt-org-uk: 无可用域名");
  }
  const domain = domains[randomInt(domains.length)];
  const username = randomUsername(10);
  const email = `${username}@${domain}`;
  const { token, gmSid } = await createInbox(email);

  return {
    channel: CHANNEL,
    email,
    token: JSON.stringify({ gmSid, inbox: token }),
  };
}

export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  if (!token) {
    throw new Error("chatgpt-org-uk: token 不能为空");
  }

  let gmSid = "";
  let inboxToken = "";
  try {
    const parsed = JSON.parse(token) as { gmSid?: string; inbox?: string };
    gmSid = parsed.gmSid || "";
    inboxToken = parsed.inbox || "";
  } catch {
    inboxToken = token;
  }

  if (!inboxToken) {
    throw new Error("chatgpt-org-uk: inbox token 缺失");
  }

  /* 如果 gmSid 丢失，重新获取 */
  if (!gmSid) {
    const sess = await createInbox(email);
    gmSid = sess.gmSid;
    inboxToken = sess.token;
  }

  const encodedEmail = encodeURIComponent(email);
  const response = await fetchWithTimeout(
    `${BASE_URL}/emails?email=${encodedEmail}`,
    {
      method: "GET",
      headers: {
        ...DEFAULT_HEADERS,
        Cookie: `gm_sid=${gmSid}`,
        "x-inbox-token": inboxToken,
      },
    },
  );

  if (!response.ok) {
    if (response.status === 401 || response.status === 403) {
      const sess = await createInbox(email);
      const retry = await fetchWithTimeout(
        `${BASE_URL}/emails?email=${encodedEmail}`,
        {
          method: "GET",
          headers: {
            ...DEFAULT_HEADERS,
            Cookie: `gm_sid=${sess.gmSid}`,
            "x-inbox-token": sess.token,
          },
        },
      );
      if (!retry.ok) {
        throw new Error(`chatgpt-org-uk: 获取邮件失败 ${retry.status}`);
      }
      const retryData = (await retry.json()) as { success: boolean; data?: { emails?: any[] } };
      if (!retryData.success) return [];
      return (retryData.data?.emails || []).map((raw: any) =>
        normalizeEmail(raw, email),
      );
    }
    throw new Error(`chatgpt-org-uk: 获取邮件失败 ${response.status}`);
  }

  const data = (await response.json()) as { success: boolean; data?: { emails?: any[] } };
  if (!data.success) return [];
  return (data.data?.emails || []).map((raw: any) =>
    normalizeEmail(raw, email),
  );
}
