import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "xkx-me";
const BASE_URL = "https://xkx.me";

/**
 * 从首页 HTML 提取 CSRF token
 */
function extractCsrf(html: string): string {
  const m = html.match(/csrf-token" content="([^"]+)"/);
  if (!m) throw new Error("xkx-me: 无法获取 CSRF token");
  return m[1];
}

/**
 * 从 Set-Cookie 头中提取 cookies
 */
function extractCookies(resp: Response): string {
  const raw = resp.headers.get("set-cookie") || "";
  if (!raw) return "";
  return raw
    .split(",")
    .map((c) => c.split(";")[0].trim())
    .filter(Boolean)
    .join("; ");
}

/**
 * 创建 xkx.me 临时邮箱
 * 1. GET / 获取 CSRF token 和 session cookie
 * 2. POST /mailbox/create/random 创建随机邮箱
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const pageResp = await fetchWithTimeout(BASE_URL, {
    method: "GET",
    headers: {
      Accept: "text/html",
      "User-Agent":
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
    },
  });
  if (!pageResp.ok) {
    throw new Error(`xkx-me: 获取首页失败 HTTP ${pageResp.status}`);
  }

  const pageHtml = await pageResp.text();
  const csrf = extractCsrf(pageHtml);
  const cookies = extractCookies(pageResp);

  const createResp = await fetchWithTimeout(`${BASE_URL}/mailbox/create/random`, {
    method: "POST",
    headers: {
      "Content-Type": "application/x-www-form-urlencoded",
      Cookie: cookies,
      "User-Agent":
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
    },
    body: `_token=${encodeURIComponent(csrf)}`,
    redirect: "manual",
  });

  const location = createResp.headers.get("location") || "";
  let body = "";
  if (!location) {
    body = await createResp.text();
  }

  const source = location || body;
  const emailMatch = source.match(/mailbox\/([^\s/"'<>]+@xkx\.me)/);
  if (!emailMatch) {
    throw new Error("xkx-me: 无法从响应中提取邮箱地址");
  }

  return {
    channel: CHANNEL,
    email: emailMatch[1],
    token: cookies,
  };
}

/**
 * 获取 xkx.me 邮件列表
 * GET /mailbox/{email}/messages (JSON)
 */
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  const address = String(email || "").trim();
  if (!address) {
    throw new Error("xkx-me: 邮箱地址为空");
  }

  const resp = await fetchWithTimeout(
    `${BASE_URL}/mailbox/${encodeURIComponent(address)}/messages`,
    {
      method: "GET",
      headers: {
        Accept: "application/json",
        "X-Requested-With": "XMLHttpRequest",
        Cookie: token || "",
        "User-Agent":
          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
      },
    },
  );

  if (resp.status === 404) return [];
  if (!resp.ok) {
    throw new Error(`xkx-me: 获取邮件失败 HTTP ${resp.status}`);
  }

  const data = await resp.json();
  const messages: Array<Record<string, unknown>> = Array.isArray(data)
    ? data
    : data?.messages || [];

  if (messages.length === 0) return [];

  return messages.map((msg) => {
    const raw: Record<string, unknown> = {
      id: msg.id,
      from: msg.from,
      to: address,
      subject: msg.subject,
      date: msg.date,
      html: msg.html || msg.body || "",
      text: msg.text || "",
    };
    return normalizeEmail(raw, address);
  });
}
