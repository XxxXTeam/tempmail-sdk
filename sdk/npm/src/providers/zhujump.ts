import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";
import { randomInt } from "crypto";

const BASE_URL = "https://mail.zhujump.com";
const LOGIN_REFERER = `${BASE_URL}/zh-CN/login`;
const TOKEN_PREFIX = "zhj1:";
const DEFAULT_EXPIRY_TIME = 60 * 60 * 1000;

const JSON_HEADERS: Record<string, string> = {
  Accept: "application/json",
  Origin: BASE_URL,
  Referer: LOGIN_REFERER,
  "User-Agent":
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
};

interface SessionToken {
  c: string;
  i: string;
  b?: string;
}

interface MoemailInstance {
  baseUrl: string;
  domain: string;
  channel: Channel;
  expiryTime?: number | null;
}

function randomValue(prefix: string, size: number): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  let out = prefix;
  for (let i = 0; i < size; i++) {
    out += chars[randomInt(chars.length)];
  }
  return out;
}

function randomPassword(): string {
  return `Sdk!${randomValue("", 12)}A1`;
}

function setCookieLines(headers: Headers): string[] {
  const h = headers as Headers & { getSetCookie?: () => string[] };
  if (typeof h.getSetCookie === "function") {
    return h.getSetCookie();
  }
  const one = headers.get("set-cookie");
  return one ? one.split(/,(?=[^ ;]+=)/) : [];
}

function mergeCookieHeader(prev: string, headers: Headers): string {
  const jar = new Map<string, string>();
  for (const part of prev.split(";")) {
    const text = part.trim();
    const idx = text.indexOf("=");
    if (idx > 0) {
      jar.set(text.slice(0, idx), text.slice(idx + 1));
    }
  }
  for (const line of setCookieLines(headers)) {
    const first = line.split(";")[0]?.trim() || "";
    const idx = first.indexOf("=");
    if (idx > 0) {
      jar.set(first.slice(0, idx), first.slice(idx + 1));
    }
  }
  return [...jar.entries()].map(([k, v]) => `${k}=${v}`).join("; ");
}

function encodeToken(token: SessionToken): string {
  return (
    TOKEN_PREFIX + Buffer.from(JSON.stringify(token), "utf8").toString("base64")
  );
}

function decodeToken(token: string): SessionToken {
  if (!token.startsWith(TOKEN_PREFIX)) {
    throw new Error("zhujump: invalid session token");
  }

  let raw = "";
  try {
    raw = Buffer.from(token.slice(TOKEN_PREFIX.length), "base64").toString(
      "utf8",
    );
  } catch {
    throw new Error("zhujump: invalid session token");
  }

  const parsed = JSON.parse(raw) as Partial<SessionToken>;
  const cookie = String(parsed.c || "").trim();
  const emailId = String(parsed.i || "").trim();
  if (!cookie || !emailId) {
    throw new Error("zhujump: invalid session token");
  }
  const baseUrl = String(parsed.b || "").trim();
  return baseUrl
    ? { c: cookie, i: emailId, b: baseUrl }
    : { c: cookie, i: emailId };
}

function loginReferer(baseUrl: string): string {
  return `${baseUrl}/zh-CN/login`;
}

function jsonHeaders(baseUrl: string): Record<string, string> {
  return {
    ...JSON_HEADERS,
    Origin: baseUrl,
    Referer: loginReferer(baseUrl),
  };
}

async function ensureLoggedIn(
  baseUrl: string,
  cookie: string,
  username: string,
): Promise<void> {
  const response = await fetchWithTimeout(`${baseUrl}/api/auth/session`, {
    headers: {
      ...jsonHeaders(baseUrl),
      Cookie: cookie,
    },
  });
  if (!response.ok) {
    throw new Error(`zhujump session: ${response.status}`);
  }

  const json = (await response.json()) as {
    user?: {
      username?: string;
    };
  };
  if (String(json.user?.username || "").trim() !== username) {
    throw new Error("zhujump: login verification failed");
  }
}

async function loginAndCreate(
  instance: MoemailInstance,
): Promise<{ email: string; emailId: string; cookie: string }> {
  const username = randomValue("sdk", 10);
  const password = randomPassword();
  let cookie = "";
  const baseUrl = instance.baseUrl.replace(/\/$/, "");
  const headers = jsonHeaders(baseUrl);

  const registerResp = await fetchWithTimeout(`${baseUrl}/api/auth/register`, {
    method: "POST",
    headers: {
      ...headers,
      "Content-Type": "application/json",
    },
    body: JSON.stringify({ username, password, turnstileToken: "" }),
  });
  if (!registerResp.ok) {
    throw new Error(`zhujump register: ${registerResp.status}`);
  }
  cookie = mergeCookieHeader(cookie, registerResp.headers);

  const csrfResp = await fetchWithTimeout(`${baseUrl}/api/auth/csrf`, {
    headers: {
      ...headers,
      Cookie: cookie,
    },
  });
  if (!csrfResp.ok) {
    throw new Error(`zhujump csrf: ${csrfResp.status}`);
  }
  cookie = mergeCookieHeader(cookie, csrfResp.headers);

  const csrfJson = (await csrfResp.json()) as { csrfToken?: string };
  const csrfToken = String(csrfJson.csrfToken || "").trim();
  if (!csrfToken) {
    throw new Error("zhujump: csrf token missing");
  }

  const loginResp = await fetchWithTimeout(
    `${baseUrl}/api/auth/callback/credentials?`,
    {
      method: "POST",
      headers: {
        ...headers,
        Cookie: cookie,
        "Content-Type": "application/x-www-form-urlencoded",
        "x-auth-return-redirect": "1",
      },
      body: new URLSearchParams({
        username,
        password,
        turnstileToken: "",
        redirect: "false",
        csrfToken,
        callbackUrl: loginReferer(baseUrl),
      }).toString(),
      redirect: "manual",
    },
  );
  if (!loginResp.ok) {
    throw new Error(`zhujump login: ${loginResp.status}`);
  }
  cookie = mergeCookieHeader(cookie, loginResp.headers);

  await ensureLoggedIn(baseUrl, cookie, username);

  const local = randomValue("sdk", 10);
  const body: Record<string, string | number> = {
    name: local,
    domain: instance.domain,
  };
  if (instance.expiryTime !== null) {
    body.expiryTime = instance.expiryTime ?? DEFAULT_EXPIRY_TIME;
  }

  const generateResp = await fetchWithTimeout(
    `${baseUrl}/api/emails/generate`,
    {
      method: "POST",
      headers: {
        ...headers,
        Cookie: cookie,
        "Content-Type": "application/json",
      },
      body: JSON.stringify(body),
    },
  );
  if (!generateResp.ok) {
    throw new Error(`zhujump generate: ${generateResp.status}`);
  }

  const generateJson = (await generateResp.json()) as {
    id?: string;
    email?: string;
  };
  const emailId = String(generateJson.id || "").trim();
  const email = String(generateJson.email || "").trim();
  if (!emailId || !email) {
    throw new Error("zhujump: invalid generate response");
  }

  return { email, emailId, cookie };
}

/**
 * 创建 mail.zhujump.com 固定域临时邮箱
 */
export async function generateEmail(
  domain: string,
  channel: Channel,
): Promise<InternalEmailInfo> {
  return generateEmailForInstance({
    baseUrl: BASE_URL,
    domain,
    channel,
    expiryTime: null,
  });
}

/**
 * 创建指定 MoeMail 部署实例的临时邮箱
 */
export async function generateEmailForInstance(
  instance: MoemailInstance,
): Promise<InternalEmailInfo> {
  const baseUrl = instance.baseUrl.replace(/\/$/, "");
  const result = await loginAndCreate({ ...instance, baseUrl });
  return {
    channel: instance.channel,
    email: result.email,
    token: encodeToken({ c: result.cookie, i: result.emailId, b: baseUrl }),
  };
}

async function fetchMessageDetail(
  baseUrl: string,
  cookie: string,
  emailId: string,
  messageId: string,
): Promise<Record<string, unknown> | null> {
  const response = await fetchWithTimeout(
    `${baseUrl}/api/emails/${encodeURIComponent(emailId)}/${encodeURIComponent(messageId)}`,
    {
      headers: {
        ...jsonHeaders(baseUrl),
        Cookie: cookie,
      },
    },
  );
  if (!response.ok) return null;
  const json = (await response.json()) as { message?: Record<string, unknown> };
  return json.message && typeof json.message === "object" ? json.message : null;
}

function hasMessageBody(row: { content?: string; html?: string }): boolean {
  return Boolean(
    String(row.content || "").trim() || String(row.html || "").trim(),
  );
}

/**
 * 获取 mail.zhujump.com 邮件列表
 */
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  const session = decodeToken(token);
  const baseUrl = (session.b || BASE_URL).replace(/\/$/, "");
  const response = await fetchWithTimeout(
    `${baseUrl}/api/emails/${encodeURIComponent(session.i)}`,
    {
      headers: {
        ...jsonHeaders(baseUrl),
        Cookie: session.c,
      },
    },
  );
  if (!response.ok) {
    throw new Error(`zhujump get emails: ${response.status}`);
  }

  const json = (await response.json()) as {
    messages?: Array<{
      id?: string;
      from_address?: string;
      to_address?: string | null;
      subject?: string;
      content?: string;
      html?: string;
      sent_at?: number;
      received_at?: number;
    }>;
  };

  const rows = Array.isArray(json.messages) ? json.messages : [];
  const emails: Email[] = [];
  for (const row of rows) {
    let source: Record<string, unknown> = row;
    const id = String(row.id || "").trim();
    if (id && !hasMessageBody(row)) {
      const detail = await fetchMessageDetail(
        baseUrl,
        session.c,
        session.i,
        id,
      );
      if (detail) {
        source = { ...row, ...detail };
      }
    }
    emails.push(
      normalizeEmail(
        {
          id: source.id || "",
          from_address: source.from_address || "",
          to_address: source.to_address || email,
          subject: source.subject || "",
          content: source.content || "",
          html: source.html || "",
          received_at: source.received_at ?? source.sent_at,
          isRead: false,
        },
        email,
      ),
    );
  }
  return emails;
}
