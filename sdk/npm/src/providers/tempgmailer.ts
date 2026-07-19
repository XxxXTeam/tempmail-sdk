/**
 * tempgmailer.com 临时邮箱渠道
 *
 * 流程：
 *   1. GET 首页获取 CSRF token 和 Laravel session cookie
 *   2. POST /get-gmail 创建 Gmail 临时邮箱
 *   3. POST /get-inbox 获取邮件列表
 */
import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "tempgmailer";
const BASE_URL = "https://tempgmailer.com";

const COMMON_HEADERS: Record<string, string> = {
  "User-Agent":
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
  Accept: "application/json, text/plain, */*",
  "Content-Type": "application/json",
  "X-Requested-With": "XMLHttpRequest",
  "X-TempGmailer-Auth": "frontend",
  Origin: BASE_URL,
  Referer: `${BASE_URL}/`,
};

/** 会话信息：CSRF token + session cookie */
interface TempGmailerSession {
  csrfToken: string;
  cookie: string;
}

/** /get-gmail 响应 */
interface CreateResponse {
  success?: boolean;
  data?: {
    email?: string;
  };
}

/** /get-inbox 中的邮件项 */
interface InboxMessage {
  from?: { address?: string; name?: string } | string;
  subject?: string;
  intro?: string;
  body?: string;
  createdAt?: string;
}

/** /get-inbox 响应 */
interface InboxResponse {
  success?: boolean;
  data?: {
    email?: string;
    messages?: InboxMessage[];
  };
}

/**
 * 从 Set-Cookie 头中提取 cookie 字符串
 */
function cookieHeader(headers: Headers): string {
  const getSetCookie = (headers as any).getSetCookie?.bind(headers);
  const values: string[] =
    typeof getSetCookie === "function"
      ? getSetCookie()
      : headers.get("set-cookie")
        ? [headers.get("set-cookie") as string]
        : [];
  return values
    .map((value) => value.split(";")[0])
    .filter(Boolean)
    .join("; ");
}

/**
 * 从 HTML 中提取 CSRF token
 * 匹配 <meta name="csrf-token" content="...">
 */
function extractCsrfToken(html: string): string {
  const match = html.match(
    /<meta\s+name="csrf-token"\s+content="([^"]+)"/,
  );
  return match ? match[1] : "";
}

/**
 * 初始化会话：获取 CSRF token 和 session cookie
 */
async function initSession(): Promise<TempGmailerSession> {
  const response = await fetchWithTimeout(BASE_URL, {
    headers: {
      Accept:
        "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
      "User-Agent": COMMON_HEADERS["User-Agent"],
    },
  });
  if (!response.ok) {
    throw new Error(`tempgmailer home: ${response.status}`);
  }

  const cookie = cookieHeader(response.headers);
  const html = await response.text();
  const csrfToken = extractCsrfToken(html);

  if (!cookie || !csrfToken) {
    throw new Error("tempgmailer: 获取 CSRF token 或 session cookie 失败");
  }

  return { csrfToken, cookie };
}

/**
 * 编码会话信息为 token 字符串
 */
function encodeSession(session: TempGmailerSession): string {
  return JSON.stringify(session);
}

/**
 * 从 token 字符串解码会话信息
 */
function decodeSession(token: string): TempGmailerSession {
  const data = JSON.parse(token) as Partial<TempGmailerSession>;
  if (!data.cookie || !data.csrfToken) {
    throw new Error("tempgmailer: 无效的 session token");
  }
  return { cookie: data.cookie, csrfToken: data.csrfToken };
}

/**
 * 创建 tempgmailer 临时邮箱
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const session = await initSession();

  const response = await fetchWithTimeout(`${BASE_URL}/get-gmail`, {
    method: "POST",
    headers: {
      ...COMMON_HEADERS,
      "X-CSRF-TOKEN": session.csrfToken,
      Cookie: session.cookie,
    },
    body: JSON.stringify({ refresh: true, adblock: 0 }),
  });

  if (!response.ok) {
    throw new Error(`tempgmailer get-gmail: ${response.status}`);
  }

  const data = (await response.json()) as CreateResponse;
  if (!data.success || !data.data?.email) {
    throw new Error("tempgmailer: 创建邮箱失败");
  }

  return {
    channel: CHANNEL,
    email: data.data.email,
    token: encodeSession(session),
  };
}

/**
 * 获取 tempgmailer 邮件列表
 */
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  const session = decodeSession(token);

  const response = await fetchWithTimeout(`${BASE_URL}/get-inbox`, {
    method: "POST",
    headers: {
      ...COMMON_HEADERS,
      "X-CSRF-TOKEN": session.csrfToken,
      Cookie: session.cookie,
    },
    body: JSON.stringify({ email, adblock: 0 }),
  });

  if (!response.ok) {
    throw new Error(`tempgmailer get-inbox: ${response.status}`);
  }

  const data = (await response.json()) as InboxResponse;
  if (!data.success) {
    throw new Error("tempgmailer: 获取收件箱失败");
  }

  const messages = data.data?.messages ?? [];
  const emails: Email[] = [];

  for (const msg of messages) {
    /* 解析发件人地址 */
    let fromAddr = "";
    if (typeof msg.from === "string") {
      fromAddr = msg.from;
    } else if (msg.from?.address) {
      fromAddr = msg.from.address;
    }

    /* body 为完整 HTML，intro 为纯文本摘要 */
    const htmlContent = msg.body ?? msg.intro ?? "";
    const textContent = msg.intro ?? "";

    emails.push(
      normalizeEmail(
        {
          from: fromAddr,
          to: email,
          subject: msg.subject ?? "",
          html: htmlContent,
          text: textContent,
          date: msg.createdAt ?? "",
        },
        email,
      ),
    );
  }

  return emails;
}
