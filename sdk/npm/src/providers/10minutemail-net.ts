import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL = "10minutemail-net" as Channel;
const API_BASE = "https://10minutemail.net";

interface AddressResponse {
  mail_get_user?: string;
  mail_get_mail?: string;
  mail_get_host?: string;
  mail_get_time?: number;
  mail_get_duetime?: number;
  mail_get_key?: string;
  mail_left_time?: number;
  mail_list?: MailListItem[];
}

interface MailListItem {
  mail_id?: string;
  from?: string;
  subject?: string;
  datetime?: string;
  isread?: boolean;
}

interface MailDetailResponse {
  from?: string;
  to?: string;
  subject?: string;
  datetime?: string;
  timestamp?: number;
  body?: Array<{ content?: string; body?: string }>;
  plain?: string[];
}

/** 从 Set-Cookie 响应头提取 PHPSESSID */
function extractPHPSESSID(response: Response): string {
  const h = response.headers as Headers & { getSetCookie?: () => string[] };
  const cookies =
    typeof h.getSetCookie === "function"
      ? h.getSetCookie()
      : (response.headers.get("set-cookie") || "").split(/,(?=[^;]*=)/);

  for (const cookie of cookies) {
    const match = cookie.match(/PHPSESSID=([^;]+)/);
    if (match) return match[1];
  }
  return "";
}

/**
 * 将邮件详情映射为标准化中间格式
 */
function flattenDetail(
  raw: MailDetailResponse,
  recipient: string,
): Record<string, unknown> {
  let textBody = "";
  let htmlBody = "";
  if (Array.isArray(raw.body)) {
    for (const part of raw.body) {
      if (part.content === "text/plain") textBody = part.body || "";
      if (part.content === "text/html") htmlBody = part.body || "";
    }
  }
  if (!textBody && Array.isArray(raw.plain) && raw.plain.length > 0) {
    textBody = raw.plain.join("\n");
  }

  return {
    id: String(raw.timestamp || ""),
    from: raw.from || "",
    to: raw.to || recipient,
    subject: raw.subject || "",
    text: textBody,
    html: htmlBody,
    date: raw.datetime || "",
  };
}

/**
 * 创建 10minutemail.net 临时邮箱
 * API: GET /address.api.php（通过 session cookie 分配邮箱）
 * token 存储为 JSON: {"cookie":"PHPSESSID=xxx"}
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const response = await fetchWithTimeout(`${API_BASE}/address.api.php`, {
    method: "GET",
    headers: { Accept: "application/json" },
  });
  if (!response.ok) {
    throw new Error(`10minutemail-net: 创建邮箱失败 ${response.status}`);
  }

  const sessId = extractPHPSESSID(response);
  if (!sessId) {
    throw new Error("10minutemail-net: 未获取到 PHPSESSID");
  }

  const data = (await response.json()) as AddressResponse;
  const address = String(data.mail_get_mail || "").trim();
  if (!address || !address.includes("@")) {
    throw new Error("10minutemail-net: 创建邮箱响应无效");
  }

  return {
    channel: CHANNEL,
    email: address,
    token: JSON.stringify({ cookie: `PHPSESSID=${sessId}` }),
  };
}

/**
 * 获取 10minutemail.net 邮件列表
 * 先获取邮件列表，再逐个获取邮件详情
 * @param token - JSON 字符串包含 cookie
 * @param email - 邮箱地址
 */
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  if (!token) {
    throw new Error("10minutemail-net: token 不能为空");
  }

  let cookie = "";
  try {
    const parsed = JSON.parse(token) as { cookie?: string };
    cookie = String(parsed.cookie || "").trim();
  } catch {
    throw new Error("10minutemail-net: token 格式无效");
  }
  if (!cookie) {
    throw new Error("10minutemail-net: token 缺少 cookie");
  }

  const listResponse = await fetchWithTimeout(`${API_BASE}/address.api.php`, {
    method: "GET",
    headers: {
      Accept: "application/json",
      Cookie: cookie,
    },
  });
  if (!listResponse.ok) {
    throw new Error(`10minutemail-net: 获取邮件列表失败 ${listResponse.status}`);
  }

  const listData = (await listResponse.json()) as AddressResponse;
  const mailList: MailListItem[] = Array.isArray(listData.mail_list)
    ? listData.mail_list.filter((m) => m.mail_id && m.mail_id !== "welcome")
    : [];

  if (mailList.length === 0) return [];

  const emails: Email[] = [];
  for (const item of mailList) {
    const detailResponse = await fetchWithTimeout(
      `${API_BASE}/mail.api.php?mailid=${encodeURIComponent(item.mail_id!)}`,
      {
        method: "GET",
        headers: {
          Accept: "application/json",
          Cookie: cookie,
        },
      },
    );
    if (!detailResponse.ok) continue;

    const detail = (await detailResponse.json()) as MailDetailResponse;
    emails.push(normalizeEmail(flattenDetail(detail, email), email));
  }

  return emails;
}
