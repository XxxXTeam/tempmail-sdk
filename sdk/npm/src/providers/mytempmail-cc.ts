import { Channel, Email, InternalEmailInfo } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "mytempmail-cc";
const BASE_URL = "https://api.mytempmail.cc";

const HEADERS: Record<string, string> = {
  Accept: "application/json",
  "Content-Type": "application/json",
  "User-Agent":
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
};

/**
 * 生成随机小写字母字符串
 * @param len - 字符串长度
 * @returns 由小写字母组成的随机字符串
 */
function randomName(len: number): string {
  const chars = "abcdefghijklmnopqrstuvwxyz";
  let result = "";
  for (let i = 0; i < len; i++) {
    result += chars.charAt(Math.floor(Math.random() * chars.length));
  }
  return result;
}

/**
 * 发送 HTTP 请求并解析 JSON 响应
 * @param path - 请求路径
 * @param init - 可选的 fetch 配置
 * @returns 解析后的 JSON 数据
 */
async function requestJson<T>(path: string, init?: RequestInit): Promise<T> {
  const response = await fetchWithTimeout(`${BASE_URL}${path}`, {
    ...init,
    headers: {
      ...HEADERS,
      ...(init?.headers || {}),
    },
  });
  const text = await response.text();
  if (!response.ok) {
    throw new Error(
      `mytempmail-cc ${path}: ${response.status} ${text.slice(0, 160)}`,
    );
  }
  if (!text) {
    return {} as T;
  }
  return JSON.parse(text) as T;
}

/**
 * 将原始邮件消息展平为归一化所需的扁平结构
 * @param raw - API 返回的原始邮件消息
 * @param recipient - 收件人邮箱地址
 * @returns 扁平化后的邮件记录
 */
function flattenMessage(
  raw: InboxMessage,
  recipient: string,
): Record<string, unknown> {
  return {
    id: raw.id == null ? "" : String(raw.id),
    from: raw.from || "",
    to: recipient,
    subject: raw.subject || "",
    body_text: raw.body_text || "",
    html: raw.body_html || "",
    received_at: raw.received_at || "",
    isRead: false,
    attachments: [],
  };
}

/**
 * 创建 mytempmail-cc 临时邮箱
 * @returns 包含邮箱地址和认证令牌的内部邮箱信息
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const name = randomName(8);
  const data = await requestJson<CreateMailboxResponse>("/api/address", {
    method: "POST",
    body: JSON.stringify({ domain: "nilvaro.com", name, expiry: 600 }),
  });
  const email = (data.address || "").trim();
  if (!email) {
    throw new Error("mytempmail-cc: empty email response");
  }
  const token = (data.token || "").trim();
  if (!token) {
    throw new Error("mytempmail-cc: empty token response");
  }
  return {
    channel: CHANNEL,
    email,
    token,
  };
}

/**
 * 获取 mytempmail-cc 邮箱的邮件列表
 * @param token - 创建邮箱时返回的认证令牌
 * @param email - 邮箱地址
 * @returns 标准化后的邮件列表
 */
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  if (!token) {
    throw new Error("mytempmail-cc: empty token");
  }

  try {
    const data = await requestJson<InboxResponse>(
      `/api/mails/${encodeURIComponent(token)}`,
    );
    const rows = Array.isArray(data.results) ? data.results : [];
    return rows.map((row) => normalizeEmail(flattenMessage(row, email), email));
  } catch (err) {
    const message = err instanceof Error ? err.message : String(err);
    if (message.includes(": 404 ")) {
      return [];
    }
    throw err;
  }
}

/**
 * 创建邮箱接口响应
 */
type CreateMailboxResponse = {
  token?: string;
  address?: string;
  expires_in?: number;
  verified?: boolean;
};

/**
 * 收件箱单条消息原始结构
 */
type InboxMessage = {
  id?: string | number;
  from?: string | null;
  subject?: string | null;
  body_text?: string | null;
  body_html?: string | null;
  received_at?: string | null;
};

/**
 * 收件箱查询响应
 */
type InboxResponse = {
  results?: InboxMessage[];
  count?: number;
  limit?: number;
  is_full?: boolean;
};
