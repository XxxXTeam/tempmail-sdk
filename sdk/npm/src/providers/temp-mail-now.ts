import { Channel, Email, InternalEmailInfo } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "temp-mail-now";
const BASE_URL = "https://temp-mail.now";

const HEADERS: Record<string, string> = {
  Accept: "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
  "User-Agent":
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
};

const JSON_HEADERS: Record<string, string> = {
  Accept: "application/json",
  "Content-Type": "application/json",
  "User-Agent":
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
};

/**
 * 从响应头中提取 set-cookie 值
 * @param response - fetch 响应对象
 * @returns 拼接后的 cookie 字符串
 */
function extractCookies(response: Response): string {
  const cookies: string[] = [];
  /* 标准 Headers API：getSetCookie() 返回数组（Node 18+）*/
  if (typeof (response.headers as any).getSetCookie === "function") {
    const setCookies: string[] = (response.headers as any).getSetCookie();
    for (const sc of setCookies) {
      const pair = sc.split(";")[0].trim();
      if (pair) cookies.push(pair);
    }
  } else {
    /* 降级：尝试 get('set-cookie') */
    const raw = response.headers.get("set-cookie");
    if (raw) {
      for (const segment of raw.split(",")) {
        const pair = segment.split(";")[0].trim();
        if (pair && pair.includes("=")) cookies.push(pair);
      }
    }
  }
  return cookies.join("; ");
}

/**
 * 创建临时邮箱
 * 1. GET /en/ 建立会话获取 session cookie
 * 2. POST /change_email 使用该 cookie 创建/更换邮箱
 * 3. 将 cookie 字符串作为 token 返回
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  /* 第一步：访问页面建立会话 */
  const initResponse = await fetchWithTimeout(`${BASE_URL}/en/`, {
    method: "GET",
    headers: HEADERS,
    redirect: "manual",
  });

  let sessionCookie = extractCookies(initResponse);
  if (!sessionCookie) {
    throw new Error("temp-mail-now: 无法获取会话 cookie");
  }

  /* 第二步：使用会话 cookie 创建邮箱 */
  const changeResponse = await fetchWithTimeout(`${BASE_URL}/change_email`, {
    method: "POST",
    headers: {
      ...JSON_HEADERS,
      Cookie: sessionCookie,
    },
  });

  /* 合并可能的新 cookie */
  const newCookies = extractCookies(changeResponse);
  if (newCookies) {
    sessionCookie = newCookies;
  }

  const text = await changeResponse.text();
  if (!changeResponse.ok) {
    throw new Error(
      `temp-mail-now /change_email: ${changeResponse.status} ${text.slice(0, 160)}`,
    );
  }

  const data = JSON.parse(text) as ChangeEmailResponse;
  const email = (data.new_email || "").trim();
  if (!email) {
    throw new Error("temp-mail-now: 创建邮箱响应中无有效邮箱地址");
  }

  return {
    channel: CHANNEL,
    email,
    token: sessionCookie,
  };
}

/**
 * 获取邮件列表
 * 使用存储的 session cookie 调用 GET /fetch_emails
 *
 * @param token - 会话 cookie 字符串
 * @param email - 邮箱地址（用于标准化）
 */
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  if (!token) {
    throw new Error("temp-mail-now: 缺少会话 cookie");
  }

  const response = await fetchWithTimeout(`${BASE_URL}/fetch_emails`, {
    method: "GET",
    headers: {
      ...JSON_HEADERS,
      Cookie: token,
    },
  });

  const text = await response.text();
  if (!response.ok) {
    throw new Error(
      `temp-mail-now /fetch_emails: ${response.status} ${text.slice(0, 160)}`,
    );
  }

  if (!text) {
    return [];
  }

  const data = JSON.parse(text) as FetchEmailsResponse;
  const rows = Array.isArray(data.emails) ? data.emails : [];

  return rows.map((row) => normalizeEmail(flattenMessage(row, email), email));
}

/**
 * 将原始邮件消息转换为标准化中间格式
 */
function flattenMessage(
  raw: RawEmail,
  recipient: string,
): Record<string, unknown> {
  return {
    id: raw.id == null ? "" : String(raw.id),
    from_addr: raw.from || "",
    to_addr: recipient,
    subject: raw.subject || "",
    body_text: raw.body || "",
    html: raw.body || "",
    received_at: raw.date || "",
    isRead: false,
    attachments: [],
  };
}

/**
 * POST /change_email 响应结构
 */
type ChangeEmailResponse = {
  new_email?: string;
};

/**
 * 单封邮件原始结构
 */
type RawEmail = {
  id?: string | number;
  from?: string | null;
  subject?: string | null;
  body?: string | null;
  date?: string | null;
};

/**
 * GET /fetch_emails 响应结构
 */
type FetchEmailsResponse = {
  emails?: RawEmail[];
  remaining_time?: number;
};
