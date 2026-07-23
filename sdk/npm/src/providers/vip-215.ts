import WebSocket from "ws";
import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "vip-215";
const BASE = "https://vip.215.im";
const API_URL = `${BASE}/api/temp-inbox`;
const WS_TICKET_URL = `${BASE}/v1/auth/ws-ticket`;
const WS_ORIGIN = BASE;

/*
 * 推送无正文时，各 SDK 统一合成 text/html（synthetic-v1），便于解析：
 * - text：首行标记 + 每行 key: value
 * - html：根节点 .tempmail-sdk-synthetic[data-tempmail-sdk-format="synthetic-v1"] + dl/dt/dd
 */
const SYNTHETIC_MARKER = "【tempmail-sdk|synthetic|vip-215|v1】";

function sanitizeOneLine(s: unknown): string {
  if (s === undefined || s === null) return "";
  return String(s)
    .replace(/\r\n|\r|\n/g, " ")
    .trim();
}

function escapeHtml(s: string): string {
  return s
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

function buildSyntheticBodies(
  recipientEmail: string,
  d: {
    id?: string;
    from?: string;
    subject?: string;
    date?: string;
    size?: number;
  },
): { text: string; html: string } {
  const id = sanitizeOneLine(d.id);
  const from = sanitizeOneLine(d.from);
  const subject = sanitizeOneLine(d.subject);
  const date = sanitizeOneLine(d.date);
  const to = sanitizeOneLine(recipientEmail);
  const size =
    typeof d.size === "number" && Number.isFinite(d.size) ? d.size : undefined;

  const lines = [
    SYNTHETIC_MARKER,
    `id: ${id}`,
    `subject: ${subject}`,
    `from: ${from}`,
    `to: ${to}`,
    `date: ${date}`,
  ];
  if (size !== undefined && size >= 0) {
    lines.push(`size: ${Math.trunc(size)}`);
  }
  const text = lines.join("\n");

  const rows: string[] = [
    ["id", id],
    ["subject", subject],
    ["from", from],
    ["to", to],
    ["date", date],
  ].map(([k, v]) => `<dt>${escapeHtml(k)}</dt><dd>${escapeHtml(v)}</dd>`);
  if (size !== undefined && size >= 0) {
    rows.push(`<dt>size</dt><dd>${escapeHtml(String(Math.trunc(size)))}</dd>`);
  }
  const html =
    `<div class="tempmail-sdk-synthetic" data-tempmail-sdk-format="synthetic-v1" data-channel="vip-215">` +
    `<dl class="tempmail-sdk-meta">${rows.join("")}</dl></div>`;

  return { text, html };
}

const USER_AGENT =
  "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/148.0.0.0 Safari/537.36 Edg/148.0.0.0";

const HOME_HEADERS: Record<string, string> = {
  "User-Agent": USER_AGENT,
  Accept:
    "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7",
  "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
  "Cache-Control": "no-cache",
  DNT: "1",
  Pragma: "no-cache",
  "Sec-CH-UA":
    '"Chromium";v="148", "Microsoft Edge";v="148", "Not/A)Brand";v="99"',
  "Sec-CH-UA-Mobile": "?0",
  "Sec-CH-UA-Platform": '"Windows"',
  "Sec-Fetch-Dest": "document",
  "Sec-Fetch-Mode": "navigate",
  "Sec-Fetch-Site": "same-origin",
  "Sec-Fetch-User": "?1",
  "Upgrade-Insecure-Requests": "1",
};

const API_HEADERS: Record<string, string> = {
  Accept: "*/*",
  "User-Agent": USER_AGENT,
  "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
  "Cache-Control": "no-cache",
  "Content-Type": "application/json",
  DNT: "1",
  Origin: BASE,
  Pragma: "no-cache",
  Referer: `${BASE}/`,
  "Sec-CH-UA":
    '"Chromium";v="148", "Microsoft Edge";v="148", "Not/A)Brand";v="99"',
  "Sec-CH-UA-Mobile": "?0",
  "Sec-CH-UA-Platform": '"Windows"',
  "Sec-Fetch-Dest": "empty",
  "Sec-Fetch-Mode": "cors",
  "Sec-Fetch-Site": "same-origin",
  "X-Locale": "zh",
};

function cookieHeaderFromResponse(headers: Headers): string {
  const h = headers as Headers & { getSetCookie?: () => string[] };
  const lines = typeof h.getSetCookie === "function" ? h.getSetCookie() : [];
  const parts: string[] = [];
  for (const line of lines) {
    const nv = line.split(";")[0].trim();
    if (nv) parts.push(nv);
  }
  if (!parts.length) {
    const single = headers.get("set-cookie");
    if (single) {
      for (const segment of single.split(/,(?=[^ =]+=)/)) {
        const nv = segment.split(";")[0].trim();
        if (nv) parts.push(nv);
      }
    }
  }
  return parts.join("; ");
}

function mergeCookieHeader(existing: string, headers: Headers): string {
  const incoming = cookieHeaderFromResponse(headers);
  if (!incoming) return existing;
  const map = new Map<string, string>();
  for (const chunk of [existing, incoming]) {
    for (const pair of chunk.split(";")) {
      const trimmed = pair.trim();
      if (!trimmed) continue;
      const eq = trimmed.indexOf("=");
      if (eq <= 0) continue;
      map.set(trimmed.slice(0, eq).trim(), trimmed.slice(eq + 1).trim());
    }
  }
  return [...map.entries()].map(([k, v]) => `${k}=${v}`).join("; ");
}

async function establishSession(): Promise<string> {
  const res = await fetchWithTimeout(`${BASE}/`, {
    method: "GET",
    headers: HOME_HEADERS,
  });
  if (!res.ok) {
    throw new Error(`vip-215: homepage failed: ${res.status}`);
  }
  const cookie = cookieHeaderFromResponse(res.headers);
  if (
    !cookie.includes("yyds_homepage_bridge=") ||
    !cookie.includes("yyds_homepage_device=")
  ) {
    throw new Error("vip-215: missing homepage cookies");
  }
  return cookie;
}

async function fetchWsTicket(jwt: string): Promise<string> {
  const res = await fetchWithTimeout(WS_TICKET_URL, {
    method: "GET",
    headers: {
      ...API_HEADERS,
      Authorization: `Bearer ${jwt}`,
    },
  });
  const rawText = await res.text();
  if (!res.ok) {
    throw new Error(`vip-215: ws-ticket failed: ${res.status} ${rawText}`);
  }
  let body: { success?: boolean; data?: { ticket?: string } };
  try {
    body = JSON.parse(rawText) as typeof body;
  } catch {
    throw new Error(`vip-215: ws-ticket non-JSON: ${rawText.slice(0, 120)}`);
  }
  if (!body?.success || !body.data?.ticket) {
    throw new Error("vip-215: invalid ws-ticket response");
  }
  return body.data.ticket;
}

type MailboxState = {
  emails: Map<string, Email>;
  ws: WebSocket | null;
  connectPromise?: Promise<void>;
};

const mailboxes = new Map<string, MailboxState>();

function getState(token: string): MailboxState {
  let st = mailboxes.get(token);
  if (!st) {
    st = { emails: new Map(), ws: null };
    mailboxes.set(token, st);
  }
  return st;
}

function wsUrl(wsTicket: string): string {
  return `wss://vip.215.im/v1/ws?token=${encodeURIComponent(wsTicket)}`;
}

function ensureWs(jwt: string, recipientEmail: string): Promise<void> {
  const st = getState(jwt);
  if (st.ws?.readyState === WebSocket.OPEN) {
    return Promise.resolve();
  }
  if (st.connectPromise) {
    return st.connectPromise;
  }

  st.connectPromise = (async () => {
    const wsTicket = await fetchWsTicket(jwt);
    await new Promise<void>((resolve, reject) => {
      const ws = new WebSocket(wsUrl(wsTicket), {
        headers: {
          "User-Agent": USER_AGENT,
          Origin: WS_ORIGIN,
        },
      });

      st.ws = ws;

      const clearConnecting = () => {
        st.connectPromise = undefined;
      };

      ws.once("open", () => {
        clearConnecting();
        resolve();
      });

      ws.once("error", (err: Error) => {
        clearConnecting();
        st.ws = null;
        reject(err);
      });

      ws.on("message", (data: WebSocket.RawData) => {
        try {
          const msg = JSON.parse(data.toString()) as {
            type?: string;
            data?: {
              id?: string;
              from?: string;
              subject?: string;
              date?: string;
              size?: number;
            };
          };
          if (msg?.type !== "message.new" || !msg.data) {
            return;
          }
          const d = msg.data;
          const syn = buildSyntheticBodies(recipientEmail, d);
          const raw = {
            id: d.id,
            from: d.from,
            subject: d.subject,
            date: d.date,
            to: recipientEmail,
            text: syn.text,
            html: syn.html,
          };
          const norm = normalizeEmail(raw, recipientEmail);
          if (norm.id) {
            st.emails.set(norm.id, norm);
          }
        } catch {
          /* 忽略非 JSON / 非预期帧 */
        }
      });

      ws.on("close", () => {
        st.ws = null;
        st.connectPromise = undefined;
      });
    });
  })();

  return st.connectPromise;
}

/**
 * 创建临时收件箱（服务端分配地址与 JWT，收件靠 WebSocket 推送）
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  let cookie = await establishSession();
  const response = await fetchWithTimeout(API_URL, {
    method: "POST",
    headers: {
      ...API_HEADERS,
      Cookie: cookie,
    },
  });
  cookie = mergeCookieHeader(cookie, response.headers);

  if (!response.ok) {
    const text = await response.text().catch(() => "");
    throw new Error(`vip-215 create inbox failed: ${response.status} ${text}`);
  }

  const body = (await response.json()) as {
    success?: boolean;
    data?: {
      address?: string;
      token?: string;
      createdAt?: string;
      expiresAt?: string;
    };
  };

  if (!body?.success || !body.data?.address || !body.data?.token) {
    throw new Error("vip-215: invalid API response");
  }

  await ensureWs(body.data.token, body.data.address);

  return {
    channel: CHANNEL,
    email: body.data.address,
    token: body.data.token,
    createdAt: body.data.createdAt,
    expiresAt: body.data.expiresAt,
  };
}

/**
 * 通过 HTTP 接口获取邮件列表
 * GET https://vip.215.im/v1/messages
 * 相比 WebSocket 推送的元数据 + 合成卡片，HTTP 接口能拿到完整正文
 */
async function fetchMessagesViaHttp(
  jwt: string,
): Promise<Array<Record<string, any>>> {
  const res = await fetchWithTimeout(`${BASE}/v1/messages`, {
    method: "GET",
    headers: {
      ...API_HEADERS,
      Authorization: `Bearer ${jwt}`,
    },
  });
  if (!res.ok) {
    throw new Error(`vip-215 messages: HTTP ${res.status}`);
  }
  const body = (await res.json()) as {
    success?: boolean;
    data?: { messages?: Array<Record<string, any>> };
  };
  if (!body?.success) {
    throw new Error("vip-215 messages: success=false");
  }
  return Array.isArray(body.data?.messages) ? body.data.messages : [];
}

/**
 * 通过 HTTP 详情接口获取单封邮件完整内容
 * GET https://vip.215.im/v1/messages/{id}
 * 用于列表条目缺正文时补拉，失败返回 null
 */
async function fetchMessageDetail(
  jwt: string,
  id: string,
): Promise<Record<string, any> | null> {
  const trimmedId = String(id || "").trim();
  if (!trimmedId) return null;
  try {
    const res = await fetchWithTimeout(
      `${BASE}/v1/messages/${encodeURIComponent(trimmedId)}`,
      {
        method: "GET",
        headers: {
          ...API_HEADERS,
          Authorization: `Bearer ${jwt}`,
        },
      },
    );
    if (!res.ok) return null;
    const body = (await res.json()) as {
      success?: boolean;
      data?: Record<string, any>;
    };
    if (!body?.success || !body.data) return null;
    return body.data;
  } catch {
    return null;
  }
}

/**
 * 判断行是否包含真实正文（用于决定是否需要补拉详情）
 */
function hasRealBody(row: Record<string, any>): boolean {
  for (const key of ["text", "body_text", "html", "body_html", "content", "body"]) {
    const v = row[key];
    if (typeof v === "string" && v.trim()) return true;
  }
  return false;
}

/**
 * 提取邮件 ID
 */
function extractMessageId(row: Record<string, any>): string {
  for (const key of ["id", "messageId", "message_id"]) {
    const v = row[key];
    if (typeof v === "string" && v.trim()) return v.trim();
    if (typeof v === "number") return String(v);
  }
  return "";
}

/**
 * 获取邮件列表（HTTP 优先，WebSocket 回退）
 * 1. GET /v1/messages 拉取完整邮件（含真实正文）
 * 2. 缺正文的条目调用 GET /v1/messages/{id} 补详情
 * 3. HTTP 失败时回退到 WebSocket 已缓存的合成卡片
 */
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  /* 确保 WebSocket 持续订阅（供实时推送） */
  ensureWs(token, email).catch(() => {
    /* WebSocket 失败不阻断 HTTP 路径 */
  });

  try {
    const messages = await fetchMessagesViaHttp(token);
    const out: Email[] = [];
    for (const row of messages) {
      const id = extractMessageId(row);
      /* 若列表条目无真实正文，补拉详情 */
      if (!hasRealBody(row) && id) {
        const detail = await fetchMessageDetail(token, id);
        if (detail) {
          for (const [k, v] of Object.entries(detail)) {
            if (v !== null && v !== undefined) {
              row[k] = v;
            }
          }
        }
      }
      if (!row.to) row.to = email;
      out.push(normalizeEmail(row, email));
    }
    return out;
  } catch {
    /* HTTP 失败时回退到 WebSocket 缓存（可能含合成卡片） */
    const st = getState(token);
    const list = Array.from(st.emails.values());
    list.sort((a, b) => (a.date || "").localeCompare(b.date || ""));
    return list;
  }
}
