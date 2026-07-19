/**
 * smailpro.com 临时邮箱渠道
 *
 * 两段式流程：
 *   1. GET https://smailpro.com/app/payload?url={目标API}[&email=&mid=] → 返回 JWT（纯文本）
 *   2. 带 JWT 调用 api.sonjj.com 对应接口（payload={JWT}）
 * 创建邮箱、获取列表、获取详情均需先取 payload 再调用 sonjj API。
 * 本渠道不需要持久化 token，token 传空字符串即可。
 */
import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "smailpro";
const BASE_URL = "https://smailpro.com";
const API_BASE_URL = "https://api.sonjj.com/v1/temp_email";

const COMMON_HEADERS: Record<string, string> = {
  "User-Agent":
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
  Accept: "application/json, text/plain, */*",
  "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
  Referer: `${BASE_URL}/`,
};

interface SmailproCreateResponse {
  email?: string;
  expired_at?: string | number;
}

interface SmailproInboxItem {
  mid?: string;
  from?: string;
  subject?: string;
  datetime?: string;
}

interface SmailproInboxResponse {
  status?: boolean;
  data?: {
    messages?: SmailproInboxItem[];
  };
}

interface SmailproMessageResponse {
  body?: string;
  textBody?: string;
}

/**
 * 获取访问 sonjj API 所需的 JWT
 * targetURL 为目标 sonjj 接口地址（未编码），extra 为附加查询参数（email、mid 等）。
 */
async function fetchPayload(
  targetURL: string,
  extra?: Record<string, string>,
): Promise<string> {
  const params = new URLSearchParams();
  params.set("url", targetURL);
  if (extra) {
    for (const [k, v] of Object.entries(extra)) params.set(k, v);
  }

  const reqURL = `${BASE_URL}/app/payload?${params.toString()}`;
  const response = await fetchWithTimeout(reqURL, {
    method: "GET",
    headers: COMMON_HEADERS,
  });
  if (!response.ok) {
    throw new Error(`smailpro payload: ${response.status}`);
  }

  /* payload 接口返回纯文本 JWT，去除可能的引号与空白 */
  let payload = (await response.text()).trim();
  payload = payload.replace(/^"+|"+$/g, "");
  if (!payload) {
    throw new Error("smailpro: payload 为空");
  }
  return payload;
}

/**
 * 携带 JWT 调用 sonjj API 并返回响应文本
 * targetURL 为目标接口地址，extra 为获取 payload 时需要的附加参数（email、mid）。
 */
async function callAPI(
  targetURL: string,
  extra: Record<string, string> | undefined,
  label: string,
): Promise<string> {
  const payload = await fetchPayload(targetURL, extra);
  const reqURL = `${targetURL}?payload=${encodeURIComponent(payload)}`;
  const response = await fetchWithTimeout(reqURL, {
    method: "GET",
    headers: COMMON_HEADERS,
  });
  if (!response.ok) {
    throw new Error(`smailpro ${label}: ${response.status}`);
  }
  return await response.text();
}

/**
 * 创建 smailpro 临时邮箱
 * 调用 sonjj create 接口，返回 {"email":"...","expired_at":...}
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const body = await callAPI(`${API_BASE_URL}/create`, undefined, "create");
  let createResp: SmailproCreateResponse;
  try {
    createResp = JSON.parse(body) as SmailproCreateResponse;
  } catch {
    throw new Error("smailpro: 解析创建响应失败");
  }

  const email = (createResp.email ?? "").trim();
  if (!email) {
    throw new Error("smailpro: 创建邮箱失败, 未返回邮箱地址");
  }

  return { channel: CHANNEL, email, expiresAt: createResp.expired_at };
}

/**
 * 获取 smailpro 邮件列表
 * 1. 调用 sonjj inbox 接口获取列表（仅含 mid、from、subject、datetime）
 * 2. 对每封邮件调用 message 接口获取正文，再统一标准化
 * token 未使用，可传空字符串。
 */
export async function getEmails(
  _token: string,
  email: string,
): Promise<Email[]> {
  const addr = email.trim();
  if (!addr) {
    throw new Error("smailpro: 邮箱地址为空");
  }

  const inboxBody = await callAPI(
    `${API_BASE_URL}/inbox`,
    { email: addr },
    "inbox",
  );
  let inboxResp: SmailproInboxResponse;
  try {
    inboxResp = JSON.parse(inboxBody) as SmailproInboxResponse;
  } catch {
    throw new Error("smailpro: 解析邮件列表失败");
  }

  const mails = inboxResp.data?.messages ?? [];
  if (mails.length === 0) {
    return [];
  }

  const emails: Email[] = [];
  for (const m of mails) {
    const mid = (m.mid ?? "").trim();
    const raw: Record<string, unknown> = {
      id: mid,
      from: m.from ?? "",
      to: addr,
      subject: m.subject ?? "",
      date: m.datetime ?? "",
    };

    /* 拉取邮件正文（含 HTML 与纯文本），失败时保留列表元信息 */
    const content = await fetchMessage(addr, mid);
    if (content.html || content.text) {
      raw.html = content.html;
      raw.text = content.text;
    }

    emails.push(normalizeEmail(raw, addr));
  }

  return emails;
}

/**
 * 获取单封邮件正文，返回 { html, text }
 * 调用 sonjj message 接口，需在 payload 参数中携带 email 与 mid。
 */
async function fetchMessage(
  email: string,
  mid: string,
): Promise<{ html: string; text: string }> {
  if (!mid) {
    return { html: "", text: "" };
  }
  try {
    const body = await callAPI(
      `${API_BASE_URL}/message`,
      { email, mid },
      "message",
    );
    const msgResp = JSON.parse(body) as SmailproMessageResponse;
    return { html: msgResp.body ?? "", text: msgResp.textBody ?? "" };
  } catch {
    return { html: "", text: "" };
  }
}
