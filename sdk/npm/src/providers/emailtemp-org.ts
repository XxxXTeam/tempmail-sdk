/**
 * emailtemp.org 临时邮箱渠道（Laravel + CSRF）
 *
 * 流程：
 *   1. GET /en 获取 session cookie + 从 HTML meta 提取 CSRF token
 *   2. POST /messages（body: _token={csrf}&captcha=）获取邮箱地址和邮件列表
 *   3. GET /view/{id} 获取单封邮件 HTML 正文
 *
 * 该站点 reCAPTCHA 已禁用，captcha 参数传空即可。
 * Go 端 token 仅存 CSRF（session 由全局 cookie jar 维护）；本端无 cookie jar，
 * 故 token 以 base64 JSON 同时存储 {csrf, cookie}，getEmails 时手动携带 Cookie。
 */
import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "emailtemp-org";
const BASE_URL = "https://emailtemp.org";

/** 从 meta 标签提取 CSRF token（<meta name="csrf-token" content="xxx">） */
const CSRF_RE = /<meta\s+name="csrf-token"\s+content="([^"]+)"/;

const USER_AGENT =
  "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

interface EmailtempOrgMessage {
  id?: string | number;
  from?: string;
  from_email?: string;
  subject?: string;
  is_seen?: boolean;
}

interface EmailtempOrgResponse {
  mailbox?: string;
  messages?: EmailtempOrgMessage[];
}

function browserHeaders(): Record<string, string> {
  return {
    "User-Agent": USER_AGENT,
    Accept:
      "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
    "Accept-Language": "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
  };
}

function ajaxHeaders(): Record<string, string> {
  return {
    "User-Agent": USER_AGENT,
    Accept: "application/json, text/javascript, */*; q=0.01",
    "Accept-Language": "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
    "X-Requested-With": "XMLHttpRequest",
    Referer: `${BASE_URL}/en`,
  };
}

/** 从 Set-Cookie 响应头提取 cookie 行（Node 下 Headers 可能有 getSetCookie） */
function setCookieLines(headers: Headers): string[] {
  const h = headers as Headers & { getSetCookie?: () => string[] };
  if (typeof h.getSetCookie === "function") {
    return h.getSetCookie();
  }
  const one = headers.get("set-cookie");
  return one ? [one] : [];
}

/** 将 Set-Cookie 合并进既有 Cookie 请求头值 */
function mergeCookieHeader(prev: string, headers: Headers): string {
  const map = new Map<string, string>();
  if (prev) {
    for (const part of prev.split(";")) {
      const eq = part.indexOf("=");
      if (eq <= 0) continue;
      map.set(part.slice(0, eq).trim(), part.slice(eq + 1).trim());
    }
  }
  for (const line of setCookieLines(headers)) {
    const first = line.split(";")[0] ?? "";
    const eq = first.indexOf("=");
    if (eq <= 0) continue;
    map.set(first.slice(0, eq).trim(), first.slice(eq + 1).trim());
  }
  return [...map.entries()].map(([k, v]) => `${k}=${v}`).join("; ");
}

/** token 编解码：内部存储 JSON {csrf, cookie} */
function encodeToken(csrf: string, cookie: string): string {
  return Buffer.from(JSON.stringify({ csrf, cookie }), "utf8").toString(
    "base64",
  );
}

function decodeToken(token: string): { csrf: string; cookie: string } {
  let raw: string;
  try {
    raw = Buffer.from(token, "base64").toString("utf8");
  } catch {
    throw new Error("emailtemp-org: token 解码失败");
  }
  const obj = JSON.parse(raw) as { csrf?: string; cookie?: string };
  if (!obj.csrf) {
    throw new Error("emailtemp-org: 缺少 CSRF token");
  }
  return { csrf: obj.csrf, cookie: obj.cookie ?? "" };
}

/** 安全提取字段为 string（数字转整数字符串） */
function asStr(v: unknown): string {
  if (v === null || v === undefined) return "";
  if (typeof v === "number") return String(Math.trunc(v));
  return String(v).trim();
}

/**
 * 创建 emailtemp.org 临时邮箱
 * 1. GET /en 获取 session cookie 和 CSRF token（meta csrf-token）
 * 2. POST /messages（body: _token={csrf}&captcha=）获取分配的邮箱地址
 * token 以 base64 JSON 存储 {csrf, cookie}。
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const r1 = await fetchWithTimeout(`${BASE_URL}/en`, {
    headers: browserHeaders(),
  });
  if (!r1.ok) {
    throw new Error(`emailtemp-org: 获取首页失败 HTTP ${r1.status}`);
  }
  let cookie = mergeCookieHeader("", r1.headers);
  const html = await r1.text();

  const m = CSRF_RE.exec(html);
  if (!m?.[1]) {
    throw new Error("emailtemp-org: 未能从首页提取 CSRF token");
  }
  const csrf = m[1];

  const form = new URLSearchParams();
  form.set("_token", csrf);
  form.set("captcha", "");

  const r2 = await fetchWithTimeout(`${BASE_URL}/messages`, {
    method: "POST",
    headers: {
      ...ajaxHeaders(),
      "Content-Type": "application/x-www-form-urlencoded",
      "X-CSRF-TOKEN": csrf,
      Cookie: cookie,
    },
    body: form.toString(),
  });
  if (!r2.ok) {
    throw new Error(`emailtemp-org: 获取邮箱失败 HTTP ${r2.status}`);
  }
  cookie = mergeCookieHeader(cookie, r2.headers);

  const data = (await r2.json()) as EmailtempOrgResponse;
  const mailbox = (data.mailbox ?? "").trim();
  if (!mailbox || !mailbox.includes("@")) {
    throw new Error(`emailtemp-org: 邮箱地址无效: ${mailbox}`);
  }

  return { channel: CHANNEL, email: mailbox, token: encodeToken(csrf, cookie) };
}

/**
 * 获取 emailtemp.org 邮件列表
 * 1. POST /messages（body: _token={csrf}&captcha=）获取 {mailbox, messages}
 * 2. 对每封邮件 GET /view/{id} 获取 HTML 正文
 */
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  const addr = email.trim();
  if (!addr) {
    throw new Error("emailtemp-org: 邮箱地址为空");
  }
  const { csrf, cookie } = decodeToken(token);

  const form = new URLSearchParams();
  form.set("_token", csrf);
  form.set("captcha", "");

  const r = await fetchWithTimeout(`${BASE_URL}/messages`, {
    method: "POST",
    headers: {
      ...ajaxHeaders(),
      "Content-Type": "application/x-www-form-urlencoded",
      "X-CSRF-TOKEN": csrf,
      ...(cookie ? { Cookie: cookie } : {}),
    },
    body: form.toString(),
  });
  if (!r.ok) {
    throw new Error(`emailtemp-org: 获取邮件列表失败 HTTP ${r.status}`);
  }

  const data = (await r.json()) as EmailtempOrgResponse;
  const messages = data.messages ?? [];
  if (messages.length === 0) {
    return [];
  }

  const emails: Email[] = [];
  for (const msg of messages) {
    const id = asStr(msg.id);
    if (!id) {
      continue;
    }

    /* from_email 为地址，from 为显示名；有显示名且不同则格式化为 "name <email>" */
    let fromAddr = asStr(msg.from_email);
    const fromName = asStr(msg.from);
    if (fromName && fromName !== fromAddr) {
      fromAddr = `${fromName} <${fromAddr}>`;
    }

    const htmlBody = await fetchView(cookie, id);

    const raw: Record<string, unknown> = {
      id,
      from: fromAddr,
      to: addr,
      subject: asStr(msg.subject),
      html: htmlBody,
      isRead: Boolean(msg.is_seen),
    };
    emails.push(normalizeEmail(raw, addr));
  }

  return emails;
}

/** 获取单封邮件的 HTML 正文（GET /view/{id}） */
async function fetchView(cookie: string, id: string): Promise<string> {
  if (!id) {
    return "";
  }
  const r = await fetchWithTimeout(
    `${BASE_URL}/view/${encodeURIComponent(id)}`,
    {
      headers: {
        ...browserHeaders(),
        Referer: `${BASE_URL}/en`,
        ...(cookie ? { Cookie: cookie } : {}),
      },
    },
  );
  if (!r.ok) {
    return "";
  }
  return await r.text();
}
