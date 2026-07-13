import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "byom";
const DOMAIN = "byom.de";
const API_BASE = "https://api.byom.de";

const DEFAULT_HEADERS: Record<string, string> = {
  Accept: "application/json, text/plain, */*",
  "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
  "Cache-Control": "no-cache",
  DNT: "1",
  Pragma: "no-cache",
  /* 不发 Referer：实测带 Referer 触发 byom API 反爬返回 403 */
  "User-Agent":
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
};

/**
 * 生成随机用户名
 */
function randomUsername(length = 10): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  let result = "";
  for (let i = 0; i < length; i++) {
    result += chars.charAt(Math.floor(Math.random() * chars.length));
  }
  return result;
}

/**
 * 创建 byom.de 临时邮箱
 * 无需 API 调用，直接生成随机用户名 + @byom.de
 */
export async function generateEmail(
  _domain?: string | null,
  channel: Channel = CHANNEL,
): Promise<InternalEmailInfo> {
  const username = randomUsername();
  const email = `${username}@${DOMAIN}`;
  return {
    channel,
    email,
  };
}

/**
 * 获取 byom.de 邮件列表
 * GET https://api.byom.de/mails/{username}
 */
export async function getEmails(email: string): Promise<Email[]> {
  if (!email?.trim()) {
    throw new Error("byom: 邮箱地址为空");
  }
  const addr = email.trim();
  const username = addr.split("@")[0];
  if (!username) {
    throw new Error("byom: 无法从邮箱地址中提取用户名");
  }
  const response = await fetchWithTimeout(
    `${API_BASE}/mails/${encodeURIComponent(username)}`,
    {
      method: "GET",
      headers: DEFAULT_HEADERS,
    },
  );
  if (!response.ok) {
    throw new Error(`byom: 获取邮件失败 HTTP ${response.status}`);
  }
  const data = await response.json();
  if (!Array.isArray(data)) {
    return [];
  }
  return data.map((raw: unknown) => normalizeEmail(raw, addr));
}
