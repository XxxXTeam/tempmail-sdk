/**
 * Mohmal 临时邮箱渠道
 * 网站: mohmal.com
 * 基于 HTML 解析，使用 connect.sid session cookie 维持会话
 */
import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "mohmal";
const BASE = "https://www.mohmal.com";

/** 从 HTML 中提取 data-email 属性的邮箱地址 */
const DATA_EMAIL_RE = /data-email="([^"]+)"/i;
/** 匹配收件箱表格中的邮件行 */
const TR_RE = /<tr[^>]*>[\s\S]*?<\/tr>/gi;
/** 提取邮件详情链接中的 ID */
const MSG_HREF_RE = /href="\/en\/message\/([^"]+)"/i;
/** 提取发件人单元格内容 */
const TD_FROM_RE = /<td[^>]*class="[^"]*sender[^"]*"[^>]*>([\s\S]*?)<\/td>/i;
/** 提取主题单元格内容 */
const TD_SUBJECT_RE =
  /<td[^>]*class="[^"]*subject[^"]*"[^>]*>([\s\S]*?)<\/td>/i;
/** 提取时间单元格内容 */
const TD_TIME_RE = /<td[^>]*class="[^"]*time[^"]*"[^>]*>([\s\S]*?)<\/td>/i;
/** 从邮件详情页提取邮件正文内容 */
const MAIL_BODY_RE =
  /<div[^>]*class="[^"]*mail-body[^"]*"[^>]*>([\s\S]*?)<\/div>/i;
/** 去除 HTML 标签 */
const TAG_RE = /<[^>]+>/g;

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

/**
 * 从 set-cookie 响应头中提取所有 cookie 行
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
 * 解析 cookie 字符串为 Map
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
 * 合并响应头中的 set-cookie 到已有 cookie 字符串
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
 * 去除 HTML 标签，返回纯文本
 */
function stripTags(s: string): string {
  return s.replace(TAG_RE, " ").replace(/\s+/g, " ").trim();
}

/**
 * HTML 实体反转义
 */
function htmlUnescape(s: string): string {
  return s
    .replace(/&amp;/g, "&")
    .replace(/&quot;/g, '"')
    .replace(/&#34;/g, '"')
    .replace(/&#39;/g, "'")
    .replace(/&lt;/g, "<")
    .replace(/&gt;/g, ">")
    .replace(/&nbsp;/g, " ");
}

/** token 前缀，用于区分不同渠道的 token 格式 */
const TOK_PREFIX = "mohmal:";

/**
 * 编码 session 信息为 token 字符串
 * @param cookie - connect.sid 等 cookie 字符串
 */
function encodeToken(cookie: string): string {
  return TOK_PREFIX + Buffer.from(cookie, "utf8").toString("base64");
}

/**
 * 解码 token 字符串为 cookie
 */
function decodeToken(tok: string): string {
  if (!tok.startsWith(TOK_PREFIX)) {
    throw new Error("mohmal: invalid session token");
  }
  try {
    return Buffer.from(tok.slice(TOK_PREFIX.length), "base64").toString("utf8");
  } catch {
    throw new Error("mohmal: invalid session token");
  }
}

/**
 * 创建 Mohmal 临时邮箱
 *
 * 流程:
 * 1. GET /en/create/random 触发创建，服务端设置 connect.sid cookie
 * 2. 跟随重定向到 /en/inbox，从 HTML 中提取 data-email 属性获取邮箱地址
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const createUrl = `${BASE}/en/create/random`;

  /* 第一步: 请求创建随机邮箱，手动处理重定向以收集 cookie */
  const r1 = await fetchWithTimeout(createUrl, {
    headers: { ...PAGE_HEADERS, Referer: `${BASE}/en` },
    redirect: "manual",
  });
  let cookieHdr = mergeCookieHeader("", r1.headers);

  /* 如果返回重定向，跟随到 /en/inbox */
  const location = r1.headers.get("location");
  if (location) {
    const inboxUrl = location.startsWith("http")
      ? location
      : `${BASE}${location}`;
    const r2 = await fetchWithTimeout(inboxUrl, {
      headers: { ...PAGE_HEADERS, Cookie: cookieHdr, Referer: createUrl },
    });
    if (!r2.ok) throw new Error(`mohmal: inbox page failed HTTP ${r2.status}`);
    cookieHdr = mergeCookieHeader(cookieHdr, r2.headers);
    const html = await r2.text();
    const email = extractEmail(html);
    const token = encodeToken(cookieHdr);
    return { channel: CHANNEL, email, token };
  }

  /* 非重定向情况，直接从响应体解析 */
  if (!r1.ok) throw new Error(`mohmal: create failed HTTP ${r1.status}`);
  const html = await r1.text();

  /* 检查是否已经是 inbox 页面 */
  const emailMatch = DATA_EMAIL_RE.exec(html);
  if (emailMatch?.[1]) {
    const email = htmlUnescape(emailMatch[1].trim());
    if (!email) throw new Error("mohmal: empty data-email");
    const token = encodeToken(cookieHdr);
    return { channel: CHANNEL, email, token };
  }

  /* 尝试请求 inbox 页面 */
  const r3 = await fetchWithTimeout(`${BASE}/en/inbox`, {
    headers: { ...PAGE_HEADERS, Cookie: cookieHdr, Referer: createUrl },
  });
  if (!r3.ok) throw new Error(`mohmal: inbox page failed HTTP ${r3.status}`);
  cookieHdr = mergeCookieHeader(cookieHdr, r3.headers);
  const inboxHtml = await r3.text();
  const email = extractEmail(inboxHtml);
  const token = encodeToken(cookieHdr);
  return { channel: CHANNEL, email, token };
}

/**
 * 从 inbox 页面 HTML 中提取邮箱地址
 */
function extractEmail(html: string): string {
  const m = DATA_EMAIL_RE.exec(html);
  if (!m?.[1]) throw new Error("mohmal: data-email not found in inbox page");
  const email = htmlUnescape(m[1].trim());
  if (!email) throw new Error("mohmal: empty data-email");
  return email;
}

/**
 * 从收件箱 HTML 表格行中解析邮件摘要信息
 */
function parseInboxRows(
  html: string,
): Array<{ id: string; from: string; subject: string; time: string }> {
  const rows: Array<{
    id: string;
    from: string;
    subject: string;
    time: string;
  }> = [];
  TR_RE.lastIndex = 0;
  let trMatch: RegExpExecArray | null;
  while ((trMatch = TR_RE.exec(html)) !== null) {
    const tr = trMatch[0];
    const hrefMatch = MSG_HREF_RE.exec(tr);
    if (!hrefMatch?.[1]) continue;
    const id = hrefMatch[1].trim();

    let from = "";
    const fromMatch = TD_FROM_RE.exec(tr);
    if (fromMatch?.[1]) from = htmlUnescape(stripTags(fromMatch[1]));

    let subject = "";
    const subjectMatch = TD_SUBJECT_RE.exec(tr);
    if (subjectMatch?.[1]) subject = htmlUnescape(stripTags(subjectMatch[1]));

    let time = "";
    const timeMatch = TD_TIME_RE.exec(tr);
    if (timeMatch?.[1]) time = htmlUnescape(stripTags(timeMatch[1]));

    rows.push({ id, from, subject, time });
  }
  return rows;
}

/**
 * 获取 Mohmal 邮箱的邮件列表
 *
 * 流程:
 * 1. GET /en/inbox 获取收件箱 HTML，解析邮件行
 * 2. 对每封邮件 GET /en/message/{id} 获取详情
 */
export async function getEmails(
  email: string,
  token: string,
): Promise<Email[]> {
  const cookieHdr = decodeToken(token);
  const inboxUrl = `${BASE}/en/inbox`;

  /* 获取收件箱页面 */
  const r = await fetchWithTimeout(inboxUrl, {
    headers: { ...PAGE_HEADERS, Cookie: cookieHdr, Referer: `${BASE}/en` },
  });
  if (!r.ok) throw new Error(`mohmal: inbox fetch failed HTTP ${r.status}`);
  const inboxHtml = await r.text();

  /* 解析邮件列表 */
  const rows = parseInboxRows(inboxHtml);
  const out: Email[] = [];
  for (const row of rows) {
    /* 获取邮件详情页 */
    const detailUrl = `${BASE}/en/message/${row.id}`;
    const rd = await fetchWithTimeout(detailUrl, {
      headers: { ...PAGE_HEADERS, Cookie: cookieHdr, Referer: inboxUrl },
    });
    if (!rd.ok) continue;
    const detailHtml = await rd.text();

    /* 提取邮件正文 */
    let htmlBody = "";
    const bodyMatch = MAIL_BODY_RE.exec(detailHtml);
    if (bodyMatch?.[1]) htmlBody = bodyMatch[1].trim();

    const raw: Record<string, unknown> = {
      id: row.id,
      from: row.from,
      to: email,
      subject: row.subject,
      date: row.time,
      html: htmlBody,
    };
    out.push(normalizeEmail(raw, email));
  }
  return out;
}
