import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "rootsh";
const ORIGIN = "https://rootsh.com";
/** 默认邮箱域名 */
const DEFAULT_DOMAIN = "bccto.cc";

/** token 前缀，用于标识序列化格式 */
const TOK_PREFIX = "rsh1:";

const COMMON_HEADERS: Record<string, string> = {
  "User-Agent":
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
  Accept: "*/*",
  "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
};

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
 * 生成随机用户名
 */
function randomLocal(length = 10): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  const bytes = new Uint8Array(length);
  crypto.getRandomValues(bytes);
  let out = "";
  for (let i = 0; i < length; i++) out += chars[bytes[i] % chars.length];
  return out;
}

/**
 * 编码会话信息为 token
 * 包含 cookie 和上次检查时间
 */
function encSess(cookie: string, time: number): string {
  const raw = JSON.stringify({ c: cookie, t: time });
  return TOK_PREFIX + Buffer.from(raw, "utf8").toString("base64");
}

/**
 * 从 token 中解码会话信息
 */
function decSess(tok: string): { c: string; t: number } {
  if (!tok.startsWith(TOK_PREFIX)) {
    throw new Error("rootsh: 无效的会话 token");
  }
  let raw: string;
  try {
    raw = Buffer.from(tok.slice(TOK_PREFIX.length), "base64").toString("utf8");
  } catch {
    throw new Error("rootsh: 无效的会话 token");
  }
  const o = JSON.parse(raw) as { c?: string; t?: number };
  const c = (o.c ?? "").trim();
  const t = o.t ?? 0;
  if (!c) throw new Error("rootsh: 无效的会话 token");
  return { c, t };
}

/**
 * 创建临时邮箱
 * 1. GET / 获取 cookie session
 * 2. POST /applymail 申请邮箱
 */
export async function generateEmail(
  domain: string = DEFAULT_DOMAIN,
): Promise<InternalEmailInfo> {
  const user = randomLocal();
  const mailAddr = `${user}@${domain}`;

  /* 第一步：GET 首页获取 session cookie */
  const r1 = await fetchWithTimeout(`${ORIGIN}/`, {
    headers: {
      ...COMMON_HEADERS,
      Referer: `${ORIGIN}/`,
    },
  });
  if (!r1.ok) throw new Error(`rootsh: 获取 session 失败: ${r1.status}`);
  await r1.text();
  let cookieHdr = mergeCookieHeader("", r1.headers);

  /* 第二步：POST /applymail 申请邮箱 */
  const r2 = await fetchWithTimeout(`${ORIGIN}/applymail`, {
    method: "POST",
    headers: {
      ...COMMON_HEADERS,
      "Content-Type": "application/x-www-form-urlencoded",
      "X-Requested-With": "XMLHttpRequest",
      Referer: `${ORIGIN}/`,
      Cookie: cookieHdr,
    },
    body: `mail=${encodeURIComponent(mailAddr)}`,
  });
  if (!r2.ok) throw new Error(`rootsh: 申请邮箱失败: ${r2.status}`);
  cookieHdr = mergeCookieHeader(cookieHdr, r2.headers);

  const data = (await r2.json()) as {
    success?: string;
    user?: string;
    time?: number;
  };
  if (data.success !== "true") {
    throw new Error("rootsh: 申请邮箱失败，服务端返回 success != true");
  }

  const email = data.user || mailAddr;
  const time = data.time || Math.floor(Date.now() / 1000);
  const token = encSess(cookieHdr, time);

  return { channel: CHANNEL, email, token };
}

/**
 * 获取邮件列表
 * 1. POST /getmail 获取邮件列表
 * 2. 对每封邮件 POST /viewmail 获取详情
 */
export async function getEmails(
  email: string,
  token: string,
): Promise<Email[]> {
  const { c: cookieHdr, t: lastTime } = decSess(token);
  const now = Date.now();

  /* 获取邮件列表 */
  const r = await fetchWithTimeout(`${ORIGIN}/getmail`, {
    method: "POST",
    headers: {
      ...COMMON_HEADERS,
      "Content-Type": "application/x-www-form-urlencoded",
      "X-Requested-With": "XMLHttpRequest",
      Referer: `${ORIGIN}/`,
      Cookie: cookieHdr,
    },
    body: `mail=${encodeURIComponent(email)}&time=${lastTime}&_=${now}`,
  });
  if (!r.ok) throw new Error(`rootsh: 获取邮件列表失败: ${r.status}`);

  const data = (await r.json()) as {
    success?: string;
    mail?: Array<[string, string, string, string, string, number]>;
    time?: number;
  };
  if (data.success !== "true") {
    throw new Error("rootsh: 获取邮件列表失败，服务端返回 success != true");
  }

  const mailList = Array.isArray(data.mail) ? data.mail : [];
  const out: Email[] = [];

  for (const item of mailList) {
    /* item 结构: [display_name, from_email, subject, date_str, mail_fid, received_time] */
    const [, fromEmail, subject, dateStr, fid] = item;

    /* 获取邮件详情 */
    let htmlContent = "";
    if (fid) {
      try {
        const dr = await fetchWithTimeout(`${ORIGIN}/viewmail`, {
          method: "POST",
          headers: {
            ...COMMON_HEADERS,
            "Content-Type": "application/x-www-form-urlencoded",
            "X-Requested-With": "XMLHttpRequest",
            Referer: `${ORIGIN}/`,
            Cookie: cookieHdr,
          },
          body: `mail=${encodeURIComponent(fid)}&to=${encodeURIComponent(email)}`,
        });
        if (dr.ok) {
          const detail = (await dr.json()) as {
            mail?: string;
            success?: string;
          };
          if (detail.success === "true") {
            htmlContent = detail.mail || "";
          }
        }
      } catch {
        /* 获取邮件详情失败，跳过 */
      }
    }

    out.push(
      normalizeEmail(
        {
          id: fid || "",
          from: fromEmail || "",
          to: email,
          subject: subject || "",
          html: htmlContent,
          date: dateStr || "",
        },
        email,
      ),
    );
  }

  return out;
}
