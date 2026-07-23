import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "haribu";
const BASE = "https://haribu.net";

const DEFAULT_HEADERS: Record<string, string> = {
  Accept:
    "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
  "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
  "Cache-Control": "no-cache",
  DNT: "1",
  Pragma: "no-cache",
  Referer: `${BASE}/`,
  "Upgrade-Insecure-Requests": "1",
  "User-Agent":
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
};

/** 从 HTML 中提取 <input id="eposta_adres" value="xxx@yevme.com"> 的正则 */
const EMAIL_INPUT_RE =
  /<input[^>]+id\s*=\s*["']eposta_adres["'][^>]+value\s*=\s*["']([^"']+)["'][^>]*>/i;
/** 备用模式：value 在 id 之前 */
const EMAIL_INPUT_RE_ALT =
  /<input[^>]+value\s*=\s*["']([^"']+@[^"']+)["'][^>]+id\s*=\s*["']eposta_adres["'][^>]*>/i;

/** 匹配邮件列表中的 <li class="mail"> 项 */
const MAIL_ITEM_RE = /<li\s+class\s*=\s*["']mail["'][^>]*>([\s\S]*?)<\/li>/gi;

/** 从邮件项中提取发件人 */
const FROM_RE =
  /<span\s+class\s*=\s*["'](?:from|gonderen)["'][^>]*>([\s\S]*?)<\/span>/i;
/** 从邮件项中提取主题 */
const SUBJECT_RE =
  /<span\s+class\s*=\s*["'](?:subject|konu)["'][^>]*>([\s\S]*?)<\/span>/i;
/** 从邮件项中提取日期 */
const DATE_RE =
  /<span\s+class\s*=\s*["'](?:date|tarih|time|zaman)["'][^>]*>([\s\S]*?)<\/span>/i;
/** 从邮件项中提取链接（邮件详情 ID） */
const MAIL_LINK_RE = /href\s*=\s*["']\/?((?:en\/)?[^"']*?\d+[^"']*)["']/i;
/** 从邮件项中提取 data-id 属性 */
const DATA_ID_RE = /data-id\s*=\s*["']([^"']+)["']/i;

/** HTML 标签清除 */
const TAG_RE = /<[^>]+>/g;

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
 * 合并已有 cookie 和新的 set-cookie 响应头
 */
function mergeCookieHeader(prev: string, headers: Headers): string {
  const map = new Map<string, string>();
  for (const part of prev.split(";")) {
    const p = part.trim();
    const i = p.indexOf("=");
    if (i > 0) {
      map.set(p.slice(0, i).trim(), p.slice(i + 1).trim());
    }
  }
  for (const line of setCookieLines(headers)) {
    const first = line.split(";")[0]?.trim() || "";
    const i = first.indexOf("=");
    if (i > 0) {
      map.set(first.slice(0, i).trim(), first.slice(i + 1).trim());
    }
  }
  return [...map.entries()].map(([k, v]) => `${k}=${v}`).join("; ");
}

/**
 * 清除 HTML 标签，返回纯文本
 */
function stripTags(s: string): string {
  return s.replace(TAG_RE, " ").replace(/\s+/g, " ").trim();
}

/**
 * 解码常见 HTML 实体
 */
function decodeEntities(s: string): string {
  return s
    .replace(/&lt;/g, "<")
    .replace(/&gt;/g, ">")
    .replace(/&quot;/g, '"')
    .replace(/&#39;/g, "'")
    .replace(/&apos;/g, "'")
    .replace(/&#(\d+);/g, (_, n) => String.fromCharCode(parseInt(n, 10)))
    .replace(/&amp;/g, "&");
}

/**
 * 从 HTML 页面中提取邮箱地址
 * 查找 <input id="eposta_adres" value="xxx@yevme.com">
 */
function parseEmailFromHtml(html: string): string {
  let m = EMAIL_INPUT_RE.exec(html);
  if (m?.[1]) {
    return decodeEntities(m[1].trim());
  }
  m = EMAIL_INPUT_RE_ALT.exec(html);
  if (m?.[1]) {
    return decodeEntities(m[1].trim());
  }
  throw new Error("haribu: 无法从 HTML 中提取邮箱地址");
}

/**
 * 创建 haribu 临时邮箱
 * 首次 GET 请求 haribu.net 时，服务器通过 session cookie 自动分配邮箱地址
 * 从返回的 HTML 中提取 <input id="eposta_adres" value="..."> 获取邮箱
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const response = await fetchWithTimeout(BASE, {
    method: "GET",
    headers: DEFAULT_HEADERS,
  });
  if (!response.ok) {
    throw new Error(`haribu: 创建邮箱失败 HTTP ${response.status}`);
  }
  const cookie = mergeCookieHeader("", response.headers);
  if (!cookie) {
    throw new Error("haribu: 未获取到 session cookie");
  }
  const html = await response.text();
  const email = parseEmailFromHtml(html);
  if (!email.includes("@")) {
    throw new Error(`haribu: 邮箱地址格式无效: ${email}`);
  }

  return {
    channel: CHANNEL,
    email,
    token: cookie,
  };
}

/**
 * 使用栈式深度匹配提取指定 class/id 的 div 完整内部 HTML，
 * 避免非贪婪正则在嵌套 div 时截断正文。
 */
function extractBodyHTML(html: string): string {
  const targets = ["mail-body", "email-body", "mail-content", "icerik", "mail_icerik", "message-body"];
  for (const target of targets) {
    const openRe = new RegExp(
      `<div\\s+(?:id|class)\\s*=\\s*["'][^"']*${target.replace(/[-_]/g, "[-_]")}[^"']*["'][^>]*>`,
      "i",
    );
    const match = openRe.exec(html);
    if (!match) continue;

    let depth = 1;
    let pos = match.index + match[0].length;
    const start = pos;

    while (pos < html.length && depth > 0) {
      const nextOpen = html.indexOf("<div", pos);
      const nextClose = html.indexOf("</div>", pos);

      if (nextClose === -1) break;
      if (nextOpen !== -1 && nextOpen < nextClose) {
        depth++;
        pos = nextOpen + 4;
      } else {
        depth--;
        if (depth === 0) {
          return html.slice(start, nextClose).trim();
        }
        pos = nextClose + 6;
      }
    }
  }
  return "";
}

/**
 * 获取 haribu 邮件列表
 * 使用 session cookie 访问首页，解析页面中的邮件列表
 * 每封邮件通过单独请求获取详细内容
 */
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  if (!token?.trim()) {
    throw new Error("haribu: session cookie 为空");
  }
  if (!email?.trim()) {
    throw new Error("haribu: 邮箱地址为空");
  }

  const cookie = token.trim();
  const addr = email.trim();

  /* 获取收件箱页面 */
  const response = await fetchWithTimeout(BASE, {
    method: "GET",
    headers: {
      ...DEFAULT_HEADERS,
      Cookie: cookie,
    },
  });
  if (!response.ok) {
    throw new Error(`haribu: 获取收件箱失败 HTTP ${response.status}`);
  }
  const html = await response.text();

  /* 解析邮件列表 */
  MAIL_ITEM_RE.lastIndex = 0;
  const emails: Email[] = [];
  let match: RegExpExecArray | null;
  let index = 0;

  while ((match = MAIL_ITEM_RE.exec(html)) !== null) {
    const item = match[1];

    /* 提取发件人 */
    const fromMatch = FROM_RE.exec(item);
    const from = fromMatch ? decodeEntities(stripTags(fromMatch[1])) : "";

    /* 提取主题 */
    const subjectMatch = SUBJECT_RE.exec(item);
    const subject = subjectMatch
      ? decodeEntities(stripTags(subjectMatch[1]))
      : "";

    /* 提取日期 */
    const dateMatch = DATE_RE.exec(item);
    const dateStr = dateMatch ? decodeEntities(stripTags(dateMatch[1])) : "";

    /* 提取邮件 ID（从链接或 data-id 属性） */
    const idMatch = DATA_ID_RE.exec(item) || MAIL_LINK_RE.exec(item);
    const mailId = idMatch ? idMatch[1].trim() : String(index);

    /* 尝试获取邮件详细内容 */
    let htmlBody = "";
    let textBody = "";
    if (mailId && mailId !== String(index)) {
      try {
        const detailUrl = mailId.startsWith("http")
          ? mailId
          : `${BASE}/${mailId.replace(/^\//, "")}`;
        const detailResp = await fetchWithTimeout(detailUrl, {
          method: "GET",
          headers: {
            ...DEFAULT_HEADERS,
            Cookie: cookie,
          },
        });
        if (detailResp.ok) {
          const detailHtml = await detailResp.text();
          /* 使用栈式解析器提取完整正文（支持嵌套 div） */
          htmlBody = extractBodyHTML(detailHtml);
          /* 回退：尝试从 iframe srcdoc 提取 */
          if (!htmlBody) {
            const iframeMatch =
              /<iframe[^>]+srcdoc\s*=\s*["']([\s\S]*?)["'][^>]*>/i.exec(
                detailHtml,
              );
            if (iframeMatch) {
              htmlBody = decodeEntities(iframeMatch[1]);
            }
          }
        }
      } catch {
        /* 获取详情失败，使用列表中的摘要 */
      }
    }

    /* 如果没有获取到详细内容，使用列表中的文本 */
    if (!htmlBody) {
      textBody = stripTags(item);
    } else {
      textBody = stripTags(htmlBody);
    }

    let isoDate = "";
    if (dateStr) {
      try {
        isoDate = new Date(dateStr).toISOString();
      } catch {
        /* 日期解析失败 */
      }
    }

    emails.push(
      normalizeEmail(
        {
          id: mailId,
          from,
          to: addr,
          subject,
          body: textBody,
          html: htmlBody,
          date: isoDate || dateStr,
          isRead: false,
        },
        addr,
      ),
    );

    index++;
  }

  return emails;
}
