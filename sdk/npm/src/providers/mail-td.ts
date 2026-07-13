import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";
import { createHash } from "crypto";

const CHANNEL: Channel = "mail-td";
const BASE_URL = "https://api.mail.td/api";
const DEFAULT_HEADERS: Record<string, string> = {
  Accept: "application/json",
  "Content-Type": "application/json",
  "User-Agent":
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
};

/**
 * 检查哈希是否满足指定前导零位数
 * @param hash - SHA-256 哈希结果
 * @param difficulty - 需要的前导零位数
 */
function checkLeadingZeros(hash: Buffer, difficulty: number): boolean {
  const fullBytes = Math.floor(difficulty / 8);
  const remainBits = difficulty % 8;
  for (let i = 0; i < fullBytes; i++) {
    if (hash[i] !== 0) return false;
  }
  if (remainBits > 0 && fullBytes < hash.length) {
    if ((hash[fullBytes] & ((255 << (8 - remainBits)) & 255)) !== 0)
      return false;
  }
  return true;
}

/**
 * 求解 Proof-of-Work
 * 算法: SHA-256(address + timestamp + nonce)，找到使哈希有 difficulty 个前导零位的 nonce
 * @param address - 邮箱地址（小写）
 * @param timestamp - Unix 秒时间戳
 * @param difficulty - 目标前导零位数
 */
function solvePoW(
  address: string,
  timestamp: number,
  difficulty: number,
): string {
  const base = address + timestamp.toString();
  let nonce = 0;
  while (nonce < 100000000) {
    const input = base + nonce.toString();
    const hash = createHash("sha256").update(input).digest();
    if (checkLeadingZeros(hash, difficulty)) {
      return nonce.toString();
    }
    nonce++;
  }
  throw new Error("mail-td: PoW 求解超时");
}

/**
 * 生成随机字符串
 */
function randomString(length: number): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  let result = "";
  for (let i = 0; i < length; i++) {
    result += chars[Math.floor(Math.random() * chars.length)];
  }
  return result;
}

/**
 * 获取可用域名列表
 */
async function getDomains(): Promise<string[]> {
  const response = await fetchWithTimeout(`${BASE_URL}/domains`, {
    method: "GET",
    headers: DEFAULT_HEADERS,
  });
  if (!response.ok) {
    throw new Error(`mail-td: 获取域名失败 ${response.status}`);
  }
  const data = (await response.json()) as {
    domains: Array<{ domain: string; pro_only: boolean }>;
  };
  return data.domains.filter((d) => !d.pro_only).map((d) => d.domain);
}

/**
 * 创建 mail.td 临时邮箱
 * 流程:
 *   1. 获取可用域名
 *   2. 生成随机用户名和密码
 *   3. 求解 PoW（SHA-256 前导零）
 *   4. POST /accounts 创建账户
 *   5. 返回 JWT token（含账户 ID）
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const domains = await getDomains();
  if (domains.length === 0) {
    throw new Error("mail-td: 无可用域名");
  }

  const domain = domains[Math.floor(Math.random() * domains.length)];
  const username = randomString(10);
  const address = `${username}@${domain}`;
  const password = randomString(20);
  const authKey = createHash("sha256").update(password).digest("hex");

  const addressLower = address.toLowerCase().trim();
  let difficulty = 15;
  let powToken: string | undefined;

  for (let retry = 0; retry < 4; retry++) {
    const timestamp = Math.floor(Date.now() / 1000);
    const nonce = solvePoW(addressLower, timestamp, difficulty);

    const body: Record<string, unknown> = {
      address,
      auth_key: authKey,
      pow: { t: timestamp, n: nonce, d: difficulty },
    };
    if (powToken) {
      (body.pow as Record<string, unknown>).token = powToken;
    }

    const response = await fetchWithTimeout(`${BASE_URL}/accounts`, {
      method: "POST",
      headers: DEFAULT_HEADERS,
      body: JSON.stringify(body),
    });

    const result = (await response.json()) as Record<string, unknown>;

    if (result.status === "retry") {
      difficulty = (result.required_difficulty as number) || difficulty + 2;
      powToken = result.token as string;
      continue;
    }

    if (!response.ok) {
      throw new Error(
        `mail-td: 创建账户失败 ${response.status} ${result.error || ""}`,
      );
    }

    if (!result.address || !result.token) {
      throw new Error("mail-td: 创建账户响应缺少必要字段");
    }

    const token = JSON.stringify({
      jwt: result.token as string,
      id: result.id as string,
    });

    return {
      channel: CHANNEL,
      email: result.address as string,
      token,
    };
  }

  throw new Error("mail-td: PoW 重试次数超限");
}

/**
 * 获取 mail.td 邮件列表
 * @param token - JSON 字符串包含 jwt 和 id
 * @param email - 邮箱地址
 */
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  if (!token) {
    throw new Error("mail-td: 缺少 token");
  }

  let jwt: string;
  let accountId: string;
  try {
    const parsed = JSON.parse(token) as { jwt: string; id: string };
    jwt = parsed.jwt;
    accountId = parsed.id;
  } catch {
    throw new Error("mail-td: token 格式无效");
  }

  const response = await fetchWithTimeout(
    `${BASE_URL}/accounts/${accountId}/messages?page=1`,
    {
      method: "GET",
      headers: {
        ...DEFAULT_HEADERS,
        Authorization: `Bearer ${jwt}`,
      },
    },
  );

  if (!response.ok) {
    throw new Error(`mail-td: 获取邮件失败 ${response.status}`);
  }

  const data = (await response.json()) as { messages: RawMessage[] };
  const messages = Array.isArray(data.messages) ? data.messages : [];

  return messages.map((msg) =>
    normalizeEmail(flattenMessage(msg, email), email),
  );
}

/**
 * 将原始消息转为标准化中间格式
 */
function flattenMessage(
  msg: RawMessage,
  recipient: string,
): Record<string, unknown> {
  return {
    id: msg.id || "",
    from: msg.from?.address || msg.from?.name || "",
    to: recipient,
    subject: msg.subject || "",
    text: msg.text || "",
    html: msg.html || "",
    received_at: msg.created_at || "",
    isRead: msg.seen ?? false,
    attachments: msg.attachments || [],
  };
}

type RawMessage = {
  id?: string;
  from?: { address?: string; name?: string };
  to?: Array<{ address?: string }>;
  subject?: string;
  text?: string;
  html?: string;
  seen?: boolean;
  created_at?: string;
  attachments?: unknown[];
};
