/**
 * tmail-link 渠道 - tmail.link (Django)
 *
 * 创建邮箱: GET / → 从 HTML 提取随机分配的邮箱地址
 * 获取邮件:
 *   1. GET /inbox/{email}/ → 获取 csrftoken cookie
 *   2. POST /inbox/{email}/ → form-data(format=json, csrfmiddlewaretoken) → JSON 邮件列表
 *   3. GET /inbox/{email}/{key}/ → 单封邮件详情 HTML
 *
 * token 存储: csrftoken cookie 值
 */

import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "tmail-link";
const BASE = "https://tmail.link";

const DEFAULT_HEADERS: Record<string, string> = {
  Accept:
    "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
  "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
  "Cache-Control": "no-cache",
  DNT: "1",
  Pragma: "no-cache",
  "User-Agent":
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
};

/** 匹配 data-default="xxx@tmail.link" 属性 */
const DATA_DEFAULT_RE = /data-default="([^"]+@tmail\.link)"/i;
/** 通用匹配 xxx@tmail.link 邮箱 */
const EMAIL_RE = /\b([a-zA-Z0-9._%+-]+@tmail\.link)\b/;

/** 从 Set-Cookie 头中提取 csrftoken 值 */
function extractCsrfToken(headers: Headers): string {
  const h = headers as Headers & { getSetCookie?: () => string[] };
  const lines: string[] =
    typeof h.getSetCookie === "function"
      ? h.getSetCookie()
      : headers.get("set-cookie")
        ? [headers.get("set-cookie")!]
        : [];

  for (const line of lines) {
    const match = /csrftoken=([^;]+)/i.exec(line);
    if (match?.[1]) return match[1].trim();
  }
  return "";
}

/**
 * 创建临时邮箱
 * 1. GET https://tmail.link/ → 从 HTML 中提取分配的邮箱地址
 * 2. GET https://tmail.link/inbox/{email}/ → 获取 csrftoken cookie
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const res = await fetchWithTimeout(`${BASE}/`, {
    method: "GET",
    headers: DEFAULT_HEADERS,
  });
  if (!res.ok) {
    throw new Error(`tmail-link: 创建邮箱失败 HTTP ${res.status}`);
  }

  const html = await res.text();

  /* 提取邮箱地址: 优先匹配 data-default，回退到通用正则 */
  let email = "";
  const dm = DATA_DEFAULT_RE.exec(html);
  if (dm?.[1]) {
    email = dm[1].trim();
  } else {
    const em = EMAIL_RE.exec(html);
    if (em?.[1]) {
      email = em[1].trim();
    }
  }
  if (!email) {
    throw new Error("tmail-link: 未从页面提取到邮箱地址");
  }

  /* 请求 inbox 页面以获取 csrftoken cookie（首页不设置 cookie） */
  const inboxRes = await fetchWithTimeout(
    `${BASE}/inbox/${encodeURIComponent(email)}/`,
    {
      method: "GET",
      headers: DEFAULT_HEADERS,
    },
  );
  if (!inboxRes.ok) {
    throw new Error(`tmail-link: 获取收件箱页面失败 HTTP ${inboxRes.status}`);
  }
  await inboxRes.text();

  const csrftoken = extractCsrfToken(inboxRes.headers);
  if (!csrftoken) {
    throw new Error("tmail-link: 未获取到 csrftoken");
  }

  return {
    channel: CHANNEL,
    email,
    token: csrftoken,
  };
}

/**
 * 获取邮件列表
 *
 * 1. GET /inbox/{email}/ → 刷新 csrftoken cookie
 * 2. POST /inbox/{email}/ → 带 csrfmiddlewaretoken 请求 JSON 邮件列表
 * 3. 对每封邮件 GET /inbox/{email}/{key}/ 获取详情
 *
 * @param token - csrftoken cookie 值
 * @param email - 邮箱地址
 */
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  if (!email?.trim()) {
    throw new Error("tmail-link: 邮箱地址为空");
  }

  const inboxUrl = `${BASE}/inbox/${encodeURIComponent(email)}/`;

  /* 步骤1: GET inbox 页面，获取最新 csrftoken */
  const getRes = await fetchWithTimeout(inboxUrl, {
    method: "GET",
    headers: {
      ...DEFAULT_HEADERS,
      Cookie: `csrftoken=${token}`,
    },
  });
  if (!getRes.ok) {
    throw new Error(`tmail-link: 获取收件箱页面失败 HTTP ${getRes.status}`);
  }

  /* 更新 csrftoken（如果服务端刷新了 cookie） */
  const freshToken = extractCsrfToken(getRes.headers) || token;

  /* 步骤2: POST inbox 页面请求 JSON 格式邮件列表 */
  const formBody = new URLSearchParams();
  formBody.set("format", "json");
  formBody.set("csrfmiddlewaretoken", freshToken);

  const postRes = await fetchWithTimeout(inboxUrl, {
    method: "POST",
    headers: {
      ...DEFAULT_HEADERS,
      "Content-Type": "application/x-www-form-urlencoded",
      Cookie: `csrftoken=${freshToken}`,
      "X-CSRFToken": freshToken,
      Referer: inboxUrl,
    },
    body: formBody.toString(),
  });
  if (!postRes.ok) {
    throw new Error(`tmail-link: 获取邮件列表失败 HTTP ${postRes.status}`);
  }

  const data = (await postRes.json()) as { messages?: any[] };
  const messages = Array.isArray(data.messages) ? data.messages : [];

  /* 步骤3: 对每封邮件获取详情页 HTML */
  const out: Email[] = [];
  for (const msg of messages) {
    const key = msg.key || msg.id || "";
    let html = "";
    let text = "";

    if (key) {
      try {
        const detailUrl = `${BASE}/inbox/${encodeURIComponent(email)}/${encodeURIComponent(key)}/`;
        const dr = await fetchWithTimeout(detailUrl, {
          method: "GET",
          headers: {
            ...DEFAULT_HEADERS,
            Cookie: `csrftoken=${freshToken}`,
            Referer: inboxUrl,
          },
        });
        if (dr.ok) {
          html = await dr.text();
        }
      } catch {
        /* 获取详情失败不影响列表返回 */
      }
    }

    out.push(
      normalizeEmail(
        {
          id: String(key),
          from: msg.sender || msg.from || "",
          to: email,
          subject: msg.subject || "",
          text,
          html,
          date: msg.date || msg.created_at || "",
        },
        email,
      ),
    );
  }

  return out;
}
