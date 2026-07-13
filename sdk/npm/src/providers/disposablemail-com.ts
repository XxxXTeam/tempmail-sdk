/**
 * disposablemail.com 临时邮箱渠道（METRONET s.r.o. 后端）
 *
 * 与 minuteinbox.com 共享相同的 PHP API 结构。
 * 流程：
 *   1. GET / 获取 session cookie + CSRF token（正则 CSRF="xxx"）
 *   2. GET /index/index?csrf_token={csrf} 创建邮箱，返回 {"email":"user@domain.com"}
 *   3. GET /index/refresh 获取邮件列表（空收件箱返回数字 0）
 *   4. GET /email/id/{id} 获取邮件 HTML 正文
 *
 * Go 端 token 仅存 CSRF（session 由全局 cookie jar 维护）；本端无 cookie jar，
 * 故 token 以 base64 JSON 同时存储 {csrf, cookie}，getEmails 时手动携带 Cookie。
 * 字段说明（捷克语）: predmet=subject, od=from, id=邮件ID, kdy=when, precteno=read status
 */
import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "disposablemail-com";
const BASE_URL = "https://www.disposablemail.com";

/** 从 HTML 中提取 CSRF token（格式: CSRF="xxx"） */
const CSRF_RE = /CSRF="([^"]+)"/;

const USER_AGENT =
  "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

interface DisposablemailRow {
  predmet?: string;
  od?: string;
  id?: string | number;
  kdy?: string;
  precteno?: string;
}

function browserHeaders(): Record<string, string> {
  return {
    "User-Agent": USER_AGENT,
    Accept:
      "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
    "Accept-Language": "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
  };
}

function ajaxHeaders(cookie: string): Record<string, string> {
  return {
    "User-Agent": USER_AGENT,
    Accept: "application/json, text/javascript, */*; q=0.01",
    "Accept-Language": "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
    "X-Requested-With": "XMLHttpRequest",
    Referer: `${BASE_URL}/`,
    ...(cookie ? { Cookie: cookie } : {}),
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
    throw new Error("disposablemail-com: token 解码失败");
  }
  const obj = JSON.parse(raw) as { csrf?: string; cookie?: string };
  return { csrf: obj.csrf ?? "", cookie: obj.cookie ?? "" };
}

/**
 * 创建 disposablemail.com 临时邮箱
 * 1. GET / 获取 session cookie 和 CSRF token（CSRF="xxx"）
 * 2. GET /index/index?csrf_token={csrf} 创建邮箱
 * token 以 base64 JSON 存储 {csrf, cookie}。
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const r1 = await fetchWithTimeout(`${BASE_URL}/`, {
    headers: browserHeaders(),
  });
  if (!r1.ok) {
    throw new Error(`disposablemail-com: 获取首页失败 HTTP ${r1.status}`);
  }
  let cookie = mergeCookieHeader("", r1.headers);
  const html = await r1.text();

  const m = CSRF_RE.exec(html);
  if (!m?.[1]) {
    throw new Error("disposablemail-com: 未能从首页提取 CSRF token");
  }
  const csrf = m[1];

  const r2 = await fetchWithTimeout(
    `${BASE_URL}/index/index?csrf_token=${encodeURIComponent(csrf)}`,
    {
      headers: ajaxHeaders(cookie),
    },
  );
  if (!r2.ok) {
    throw new Error(`disposablemail-com: 创建邮箱失败 HTTP ${r2.status}`);
  }
  cookie = mergeCookieHeader(cookie, r2.headers);

  const data = (await r2.json()) as { email?: string };
  const email = (data.email ?? "").trim();
  if (!email || !email.includes("@")) {
    throw new Error(`disposablemail-com: 获取到的邮箱地址无效: ${email}`);
  }

  return { channel: CHANNEL, email, token: encodeToken(csrf, cookie) };
}

/**
 * 获取 disposablemail.com 邮件列表
 * 1. GET /index/refresh 获取邮件列表（空收件箱返回数字 0）
 * 2. 对每封邮件 GET /email/id/{id} 获取 HTML 正文
 */
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  const addr = email.trim();
  if (!addr) {
    throw new Error("disposablemail-com: 邮箱地址为空");
  }
  const { cookie } = decodeToken(token);

  const r = await fetchWithTimeout(`${BASE_URL}/index/refresh`, {
    headers: ajaxHeaders(cookie),
  });
  if (!r.ok) {
    throw new Error(`disposablemail-com: 获取邮件列表失败 HTTP ${r.status}`);
  }

  /* 空收件箱返回数字 0 或空数组 */
  const trimmed = (await r.text()).trim();
  if (trimmed === "0" || trimmed === "" || trimmed === "[]") {
    return [];
  }

  let list: DisposablemailRow[];
  try {
    const parsed = JSON.parse(trimmed);
    list = Array.isArray(parsed) ? parsed : [];
  } catch {
    return [];
  }
  if (list.length === 0) {
    return [];
  }

  const emails: Email[] = [];
  for (const item of list) {
    const id = String(item.id ?? "");
    if (!id) {
      continue;
    }
    const htmlBody = await fetchBody(cookie, id);

    const raw: Record<string, unknown> = {
      id,
      from: item.od ?? "",
      to: addr,
      subject: item.predmet ?? "",
      html: htmlBody,
      date: item.kdy ?? "",
      isRead: item.precteno === "precteno",
    };
    emails.push(normalizeEmail(raw, addr));
  }

  return emails;
}

/** 获取单封邮件的 HTML 正文（GET /email/id/{id}） */
async function fetchBody(cookie: string, id: string): Promise<string> {
  if (!id) {
    return "";
  }
  const r = await fetchWithTimeout(`${BASE_URL}/email/id/${id}`, {
    headers: ajaxHeaders(cookie),
  });
  if (!r.ok) {
    return "";
  }
  return await r.text();
}
