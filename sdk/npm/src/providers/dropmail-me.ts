import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL = "dropmail-me" as Channel;
const API_BASE = "https://dropmail.me/api/graphql";

/**
 * FNV-1a 变体哈希
 */
function fnvHash(str: string): string {
  let hash = 2166136261;
  for (let i = 0; i < str.length; i++) {
    hash ^= str.charCodeAt(i);
    hash += (hash << 1) + (hash << 4) + (hash << 7) + (hash << 8) + (hash << 24);
    hash &= 0xffffffff;
  }
  return (hash >>> 0).toString(16);
}

/**
 * 生成随机字母数字字符串
 */
function randomAlphanumeric(length: number): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  let result = "";
  for (let i = 0; i < length; i++) {
    result += chars[Math.floor(Math.random() * chars.length)];
  }
  return result;
}

/**
 * 生成 dropmail.me 认证 token
 */
function generateAuthToken(): string {
  const now = new Date();
  const dateStr =
    now.getFullYear().toString() +
    String(now.getMonth() + 1).padStart(2, "0") +
    String(now.getDate()).padStart(2, "0");
  const randomPart = dateStr + randomAlphanumeric(16);
  const hash = fnvHash(randomPart);
  return `website_${randomPart}_${hash}`;
}

interface DropmailMail {
  id?: string;
  headerFrom?: string;
  headerSubject?: string;
  text?: string;
  html?: string;
  receivedAt?: string;
}

/**
 * 将 dropmail.me 原始邮件映射为标准化中间格式
 */
function flattenMail(
  raw: DropmailMail,
  recipient: string,
): Record<string, unknown> {
  return {
    id: raw.id || "",
    from: raw.headerFrom || "",
    to: recipient,
    subject: raw.headerSubject || "",
    text: raw.text || "",
    html: raw.html || "",
    date: raw.receivedAt || "",
  };
}

/**
 * 创建 dropmail.me 临时邮箱
 * 使用 GraphQL mutation introduceSession 创建会话
 * token 存储为 JSON: {"session_id":"...","auth_token":"website_..."}
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const authToken = generateAuthToken();
  const query = `mutation { introduceSession { id expiresAt addresses { address } } }`;

  const response = await fetchWithTimeout(`${API_BASE}/${authToken}`, {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
      Accept: "application/json",
    },
    body: JSON.stringify({ query }),
  });
  if (!response.ok) {
    throw new Error(`dropmail-me: 创建邮箱失败 ${response.status}`);
  }

  const json = (await response.json()) as any;
  const session = json?.data?.introduceSession;
  if (!session?.id || !session?.addresses?.length) {
    throw new Error("dropmail-me: 创建邮箱响应无效");
  }

  const address = String(session.addresses[0].address || "").trim();
  if (!address || !address.includes("@")) {
    throw new Error("dropmail-me: 返回的邮箱地址无效");
  }

  return {
    channel: CHANNEL,
    email: address,
    token: JSON.stringify({ session_id: session.id, auth_token: authToken }),
    expiresAt: session.expiresAt,
  };
}

/**
 * 获取 dropmail.me 邮件列表
 * 使用 GraphQL query session 查询邮件
 * @param token - JSON 字符串包含 session_id 和 auth_token
 * @param email - 邮箱地址
 */
export async function getEmails(token: string, email: string): Promise<Email[]> {
  if (!token) {
    throw new Error("dropmail-me: token 不能为空");
  }

  let sessionId = "";
  let authToken = "";
  try {
    const parsed = JSON.parse(token) as { session_id?: string; auth_token?: string };
    sessionId = String(parsed.session_id || "").trim();
    authToken = String(parsed.auth_token || "").trim();
  } catch {
    throw new Error("dropmail-me: token 格式无效");
  }
  if (!sessionId || !authToken) {
    throw new Error("dropmail-me: token 缺少 session_id 或 auth_token");
  }

  const query = `{ session(id:"${sessionId}") { mails { id headerFrom headerSubject text html receivedAt } } }`;

  const response = await fetchWithTimeout(`${API_BASE}/${authToken}`, {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
      Accept: "application/json",
    },
    body: JSON.stringify({ query }),
  });
  if (!response.ok) {
    throw new Error(`dropmail-me: 获取邮件失败 ${response.status}`);
  }

  const json = (await response.json()) as any;
  const mails: DropmailMail[] = Array.isArray(json?.data?.session?.mails)
    ? json.data.session.mails
    : [];

  return mails.map((mail) => normalizeEmail(flattenMail(mail, email), email));
}
