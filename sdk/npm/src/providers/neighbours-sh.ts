import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL = "neighbours-sh" as Channel;
const API_BASE = "https://neighbours.sh/api/v1";
const DOMAIN = "neighbours.sh";

interface InboxListResponse {
  success?: boolean;
  data?: Array<{ uid?: number; subject?: string; from?: unknown; date?: string }>;
}

interface MessageDetailResponse {
  success?: boolean;
  data?: {
    uid?: number;
    from?: { value?: Array<{ address?: string; name?: string }>; text?: string };
    to?: { text?: string };
    subject?: string;
    text?: string;
    html?: string;
    date?: string;
    attachments?: unknown[];
  };
}

/**
 * 生成随机用户名，前缀 sdk + 10 位小写字母数字
 */
function randomUsername(): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  let out = "sdk";
  for (let i = 0; i < 10; i++) {
    out += chars[Math.floor(Math.random() * chars.length)];
  }
  return out;
}

/**
 * 将 neighbours.sh 邮件详情映射为标准化中间格式
 */
function flattenMessage(
  detail: NonNullable<MessageDetailResponse["data"]>,
  recipient: string,
): Record<string, unknown> {
  const fromValue = detail.from?.value?.[0];
  return {
    id: detail.uid != null ? String(detail.uid) : "",
    from: fromValue?.address || detail.from?.text || "",
    to: detail.to?.text || recipient,
    subject: detail.subject || "",
    text: detail.text || "",
    html: detail.html || "",
    date: detail.date || "",
    attachments: detail.attachments || [],
  };
}

/**
 * 创建 neighbours.sh 临时邮箱
 * 公共收件箱，任意用户名即可收信，无需注册
 * token 存储邮箱地址
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const email = `${randomUsername()}@${DOMAIN}`;
  return {
    channel: CHANNEL,
    email,
    token: email,
  };
}

/**
 * 获取 neighbours.sh 邮件列表
 * API: GET /inbox/{address} → 摘要列表；GET /inbox/{address}/{uid} → 单邮件详情
 * @param token - 邮箱地址
 * @param email - 邮箱地址
 */
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  const address = String(token || email || "").trim();
  if (!address) {
    throw new Error("neighbours-sh: 缺少邮箱地址");
  }

  const listResp = await fetchWithTimeout(
    `${API_BASE}/inbox/${encodeURIComponent(address)}`,
    { method: "GET", headers: { Accept: "application/json" } },
  );
  if (!listResp.ok) {
    throw new Error(`neighbours-sh: 获取邮件列表失败 ${listResp.status}`);
  }

  const listData = (await listResp.json()) as InboxListResponse;
  const rows = Array.isArray(listData.data) ? listData.data : [];

  const emails: Email[] = [];
  for (const row of rows) {
    if (row.uid == null) continue;
    /* 详情接口返回完整正文 */
    const detailResp = await fetchWithTimeout(
      `${API_BASE}/inbox/${encodeURIComponent(address)}/${row.uid}`,
      { method: "GET", headers: { Accept: "application/json" } },
    );
    if (!detailResp.ok) continue;
    const detailData = (await detailResp.json()) as MessageDetailResponse;
    if (detailData.data) {
      emails.push(
        normalizeEmail(flattenMessage(detailData.data, address), address),
      );
    }
  }
  return emails;
}
