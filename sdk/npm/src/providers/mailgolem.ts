/**
 * MailGolem 临时邮箱渠道
 * 网站: mailgolem.com
 * 域名: @mailgolem.com
 * 基于 Cookie Session + CSRF Token 维持会话
 */
import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "mailgolem";
const BASE = "https://mailgolem.com";

/** 从 HTML 中提取 CSRF token: <input type="hidden" name="_token" id="token" value="..."> */
const CSRF_RE = /name="_token"\s+id="token"\s+value="([^"]+)"/i;

/** 浏览器模拟请求头 */
const PAGE_HEADERS: Record<string, string> = {
  Accept:
    "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
  "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
  "Cache-Control": "no-cache",
  DNT: "1",
  Pragma: "no-cache",
  "Upgrade-Insecure-Requests": "1",
  "User-Agent":
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
};

/** token 前缀，用于区分不同渠道的 token 格式 */
const TOK_PREFIX = "mailgolem:";

/**
 * 从 Set-Cookie 响应头中提取所有 cookie 行
 */
function setCookieLines(headers: Headers): string[] {
  const h = headers as Headers & { getSetCookie?: () => string[] };
  if (typeof h.getSetCookie === "function") {
    return h.getSetCookie();
  }
  const one = headers.get("set-cookie");
  return one ? [one] : [];
}

/**
 * 解析 cookie 字符串为键值对 Map
 */
function parseCookieMap(hdr: string): Map<string, string> {
  const m = new Map<string, string>();
  for (const part of hdr.split(";")) {
    const p = part.trim();
    if (!p || !p.includes("=")) continue;
    const i = p.indexOf("=");
    const k = p.slice(0, i).trim();
    const v = p.slice(i + 1).trim();
    if (k) m.set(k, v);
  }
  return m;
}

/**
 * 合并已有 cookie 和新的 Set-Cookie 响应头
 */
function mergeCookieHeader(prev: string, headers: Headers): string {
  const m = parseCookieMap(prev);
  for (const line of setCookieLines(headers)) {
    const first = line.split(";")[0] ?? "";
    const i = first.indexOf("=");
    if (i <= 0) continue;
    const k = first.slice(0, i).trim();
    const v = first.slice(i + 1).trim();
    if (k) m.set(k, v);
  }
  return [...m.entries()]
    .sort((a, b) => a[0].localeCompare(b[0]))
    .map(([k, v]) => `${k}=${v}`)
    .join("; ");
}

/**
 * 编码 session 信息为 token 字符串
 * 包含 cookie 和 CSRF token
 * @param cookie - session cookie 字符串
 * @param csrf - CSRF token
 */
function encodeToken(cookie: string, csrf: string): string {
  const raw = JSON.stringify({ c: cookie, t: csrf });
  return TOK_PREFIX + Buffer.from(raw, "utf8").toString("base64");
}

/**
 * 从 token 中解码 session 信息
 */
function decodeToken(tok: string): { cookie: string; csrf: string } {
  if (!tok.startsWith(TOK_PREFIX)) {
    throw new Error("mailgolem: 无效的会话 token");
  }
  let raw: string;
  try {
    raw = Buffer.from(tok.slice(TOK_PREFIX.length), "base64").toString("utf8");
  } catch {
    throw new Error("mailgolem: 无效的会话 token");
  }
  const o = JSON.parse(raw) as { c?: string; t?: string };
  const cookie = (o.c ?? "").trim();
  const csrf = (o.t ?? "").trim();
  if (!cookie || !csrf) throw new Error("mailgolem: 无效的会话 token");
  return { cookie, csrf };
}

/**
 * 获取 session cookie 和 CSRF token
 * GET https://mailgolem.com/ 获取 HTML 并提取
 * @param existingCookie - 可选的已有 cookie，用于复用 session
 */
async function fetchSession(
  existingCookie?: string,
): Promise<{ cookie: string; csrf: string }> {
  const headers: Record<string, string> = {
    ...PAGE_HEADERS,
    Referer: `${BASE}/`,
  };
  if (existingCookie) {
    headers["Cookie"] = existingCookie;
  }

  const r = await fetchWithTimeout(`${BASE}/`, {
    headers,
  });
  if (!r.ok) {
    throw new Error(`mailgolem: 获取 session 失败 HTTP ${r.status}`);
  }

  const cookieHdr = mergeCookieHeader(existingCookie || "", r.headers);
  const html = await r.text();

  /* 从 HTML 中提取 CSRF token */
  const csrfMatch = CSRF_RE.exec(html);
  if (!csrfMatch?.[1]) {
    throw new Error("mailgolem: 无法从页面中提取 CSRF token");
  }
  const csrf = csrfMatch[1].trim();

  return { cookie: cookieHdr, csrf };
}

/**
 * 创建 MailGolem 临时邮箱
 *
 * 流程:
 * 1. GET / 获取 session cookie + CSRF token
 * 2. GET /random-email-address 获取随机邮箱地址（带 cookie）
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  /* 第一步: 获取 session 和 CSRF token */
  const { cookie, csrf } = await fetchSession();

  /* 第二步: 获取随机邮箱地址 */
  const r = await fetchWithTimeout(`${BASE}/random-email-address`, {
    headers: {
      ...PAGE_HEADERS,
      Cookie: cookie,
      Referer: `${BASE}/`,
    },
  });
  if (!r.ok) {
    throw new Error(`mailgolem: 获取随机邮箱失败 HTTP ${r.status}`);
  }

  const emailAddr = (await r.text()).trim();
  if (!emailAddr || !emailAddr.includes("@")) {
    throw new Error("mailgolem: 返回的邮箱地址无效");
  }

  const token = encodeToken(cookie, csrf);
  return { channel: CHANNEL, email: emailAddr, token };
}

/**
 * 获取 MailGolem 邮箱的邮件列表
 *
 * 流程:
 * 1. GET / 获取新的 session + CSRF token（复用不可靠，重新获取保证有效性）
 * 2. POST /fetch-emails/{email} 获取邮件列表
 */
export async function getEmails(
  email: string,
  token: string,
): Promise<Email[]> {
  if (!email?.trim()) {
    throw new Error("mailgolem: 邮箱地址为空");
  }

  /* 解码已有 token 获取 cookie，用于复用 session */
  const prev = decodeToken(token);

  /* 重新获取 session 和 CSRF token，传入已有 cookie 尝试复用 */
  const { cookie, csrf } = await fetchSession(prev.cookie);

  /* POST 获取邮件列表 */
  const addr = email.trim();
  const r = await fetchWithTimeout(
    `${BASE}/fetch-emails/${encodeURIComponent(addr)}`,
    {
      method: "POST",
      headers: {
        ...PAGE_HEADERS,
        "Content-Type": "application/x-www-form-urlencoded",
        "X-Requested-With": "XMLHttpRequest",
        Cookie: cookie,
        Referer: `${BASE}/`,
      },
      body: `_token=${encodeURIComponent(csrf)}`,
    },
  );
  if (!r.ok) {
    throw new Error(`mailgolem: 获取邮件失败 HTTP ${r.status}`);
  }

  const data = await r.json();
  if (!Array.isArray(data)) {
    return [];
  }

  return data.map((raw: unknown) => normalizeEmail(raw, addr));
}
