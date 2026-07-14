import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL = "tempmail-fish" as Channel;
const API_BASE = "https://api.tempmail.fish";

interface NewEmailResponse {
  email?: string;
  authKey?: string;
  emails?: unknown[];
}

interface FishMessage {
  to?: string;
  from?: string;
  subject?: string;
  textBody?: string;
  htmlBody?: string;
  attachments?: unknown[];
  timestamp?: number;
}

/**
 * 将 tempmail.fish 原始邮件映射为标准化中间格式
 */
function flattenMessage(
  raw: FishMessage,
  recipient: string,
): Record<string, unknown> {
  const body = raw.textBody || "";
  return {
    from: raw.from || "",
    to: raw.to || recipient,
    subject: raw.subject || "",
    /* textBody 中常内嵌 HTML，交由 normalizeEmail 自动识别归位 */
    text: body,
    html: raw.htmlBody || "",
    date: raw.timestamp ? new Date(raw.timestamp).toISOString() : "",
    attachments: raw.attachments || [],
  };
}

/**
 * 创建 tempmail.fish 临时邮箱
 * API: POST /emails/new-email → {"email":"xxx","authKey":"xxx","emails":[]}
 * token 存储为 JSON: {"email":"...","authKey":"..."}
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const response = await fetchWithTimeout(`${API_BASE}/emails/new-email`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
  });
  if (!response.ok) {
    throw new Error(`tempmail-fish: 创建邮箱失败 ${response.status}`);
  }

  const data = (await response.json()) as NewEmailResponse;
  const email = String(data.email || "").trim();
  const authKey = String(data.authKey || "").trim();
  if (!email || !email.includes("@") || !authKey) {
    throw new Error("tempmail-fish: 创建邮箱响应无效");
  }

  return {
    channel: CHANNEL,
    email,
    token: JSON.stringify({ email, authKey }),
  };
}

/**
 * 获取 tempmail.fish 邮件列表
 * API: GET /emails/emails?emailAddress={email} 头部 Authorization: {authKey}
 * @param token - JSON 字符串包含 email 和 authKey
 * @param email - 邮箱地址
 */
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  if (!token) {
    throw new Error("tempmail-fish: token 不能为空");
  }

  let address = "";
  let authKey = "";
  try {
    const parsed = JSON.parse(token) as { email?: string; authKey?: string };
    address = String(parsed.email || email || "").trim();
    authKey = String(parsed.authKey || "").trim();
  } catch {
    throw new Error("tempmail-fish: token 格式无效");
  }
  if (!address || !authKey) {
    throw new Error("tempmail-fish: token 缺少 email 或 authKey");
  }

  const response = await fetchWithTimeout(
    `${API_BASE}/emails/emails?emailAddress=${encodeURIComponent(address)}`,
    {
      method: "GET",
      headers: { Accept: "application/json", Authorization: authKey },
    },
  );
  if (!response.ok) {
    throw new Error(`tempmail-fish: 获取邮件失败 ${response.status}`);
  }

  const data = await response.json();
  const rows: FishMessage[] = Array.isArray(data)
    ? data
    : Array.isArray((data as { emails?: FishMessage[] })?.emails)
      ? (data as { emails: FishMessage[] }).emails
      : [];

  return rows.map((row) => normalizeEmail(flattenMessage(row, address), address));
}
