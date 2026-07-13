import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "tempinbox";
const BASE = "https://endpoint.tempinbox.xyz";

/** 已知可用域名 */
const KNOWN_DOMAINS = ["tempinbox.xyz", "thepiratebay.cloud", "cryptoblad.nl"];

const DEFAULT_HEADERS: Record<string, string> = {
  Accept: "application/json, text/plain, */*",
  "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
  "Cache-Control": "no-cache",
  DNT: "1",
  Pragma: "no-cache",
  Referer: "https://www.tempinbox.xyz/",
  "sec-ch-ua":
    '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
  "sec-ch-ua-mobile": "?0",
  "sec-ch-ua-platform": '"Windows"',
  "sec-fetch-dest": "empty",
  "sec-fetch-mode": "cors",
  "sec-fetch-site": "cross-site",
  "User-Agent":
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
};

/**
 * 生成随机用户名
 */
function randomUser(len = 10): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  let result = "";
  for (let i = 0; i < len; i++) {
    result += chars[Math.floor(Math.random() * chars.length)];
  }
  return result;
}

/**
 * 创建临时邮箱
 * 无指定域名时调用 GET /email/Random 获取随机地址；
 * 指定域名时使用 GET /email/{user}@{domain} 创建自定义邮箱
 */
export async function generateEmail(
  domain?: string | null,
  channel: Channel = CHANNEL,
): Promise<InternalEmailInfo> {
  let email: string;

  if (domain && typeof domain === "string") {
    const d = domain.trim().toLowerCase();
    if (!KNOWN_DOMAINS.includes(d)) {
      throw new Error(`tempinbox: 不支持的域名: ${d}`);
    }
    /* 指定域名 → 生成随机用户名并创建 */
    const user = randomUser();
    const addr = `${user}@${d}`;
    const response = await fetchWithTimeout(
      `${BASE}/email/${encodeURIComponent(addr)}`,
      {
        method: "GET",
        headers: DEFAULT_HEADERS,
      },
    );
    if (!response.ok) {
      throw new Error(`tempinbox: 创建自定义邮箱失败 HTTP ${response.status}`);
    }
    email = addr;
  } else {
    /* 未指定域名 → 随机邮箱 */
    const response = await fetchWithTimeout(`${BASE}/email/Random`, {
      method: "GET",
      headers: DEFAULT_HEADERS,
    });
    if (!response.ok) {
      throw new Error(`tempinbox: 获取随机邮箱失败 HTTP ${response.status}`);
    }
    const text = await response.text();
    /* 接口返回带引号的纯字符串，如 "user@domain" */
    email = text.replace(/^"|"$/g, "").trim();
  }

  if (!email || !email.includes("@")) {
    throw new Error("tempinbox: 返回的邮箱地址无效");
  }

  return {
    channel,
    email,
  };
}

/**
 * 获取邮件列表
 * GET /messages/{email} → 对每封邮件调用 normalize 标准化
 */
export async function getEmails(email: string): Promise<Email[]> {
  if (!email?.trim()) {
    throw new Error("tempinbox: 邮箱地址为空");
  }
  const addr = email.trim();
  const response = await fetchWithTimeout(
    `${BASE}/messages/${encodeURIComponent(addr)}`,
    {
      method: "GET",
      headers: DEFAULT_HEADERS,
    },
  );
  if (!response.ok) {
    throw new Error(`tempinbox: 获取邮件失败 HTTP ${response.status}`);
  }
  const data = await response.json();
  if (!Array.isArray(data)) {
    return [];
  }
  return data.map((raw: unknown) => normalizeEmail(raw, addr));
}
