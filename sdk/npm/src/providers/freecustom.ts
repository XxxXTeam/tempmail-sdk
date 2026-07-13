import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "freecustom";
const SITE_URL = "https://www.freecustom.email";
const REFERER = "https://www.freecustom.email/en";

const UA =
  "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36";

function randomLocal(n: number): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  let r = "";
  for (let i = 0; i < n; i++) {
    r += chars[Math.floor(Math.random() * chars.length)];
  }
  return r;
}

/**
 * 获取匿名访问令牌（JWT，有效期约 2 小时）
 * POST /api/auth → { token }
 */
async function fetchAuthToken(): Promise<string> {
  const resp = await fetchWithTimeout(`${SITE_URL}/api/auth`, {
    method: "POST",
    headers: {
      "User-Agent": UA,
      Accept: "application/json",
      "Content-Type": "application/json",
      Referer: REFERER,
    },
  });
  if (!resp.ok) {
    throw new Error(`freecustom: 获取令牌失败 ${resp.status}`);
  }
  const data = (await resp.json()) as { token?: string };
  if (!data.token) {
    throw new Error("freecustom: 令牌响应无效");
  }
  return data.token;
}

/**
 * 挑选一个当前可收信的域名。
 * api2 /domains 无需鉴权，返回域名及过期标记，优先选择未过期（expiring_soon 非 true）的域名。
 */
async function pickDomain(): Promise<string> {
  const resp = await fetchWithTimeout("https://api2.freecustom.email/domains", {
    method: "GET",
    headers: { "User-Agent": UA, Accept: "application/json", Referer: REFERER },
  });
  if (!resp.ok) {
    throw new Error(`freecustom: 获取域名列表失败 ${resp.status}`);
  }
  const data = (await resp.json()) as {
    success?: boolean;
    data?: FreeDomain[];
  };
  const list = Array.isArray(data.data) ? data.data : [];
  if (list.length === 0) {
    throw new Error("freecustom: 域名列表为空");
  }
  // 优先未过期域名；若全部标记过期则退回全量列表
  const usable = list.filter(
    (d) => d.tier === "free" && d.expiring_soon !== true,
  );
  const pool = usable.length > 0 ? usable : list;
  const picked = pool[Math.floor(Math.random() * pool.length)];
  return picked.domain;
}

/**
 * 创建 freecustom.email 临时邮箱
 * 该服务免注册，任意 local part @ 可用域名即时可收信。
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const domain = await pickDomain();
  const email = `${randomLocal(10)}@${domain}`;
  return {
    channel: CHANNEL,
    email,
    token: email,
  };
}

/**
 * 获取 freecustom.email 邮件列表
 * 1. POST /api/auth 取 JWT
 * 2. GET /api/public-mailbox?fullMailboxId=<email> 取邮件元数据列表
 * 3. 对每封 GET /api/public-mailbox?fullMailboxId=<email>&messageId=<id> 补全正文
 */
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  const addr = token || email;
  if (!addr) {
    throw new Error("freecustom: 缺少邮箱地址");
  }

  const jwt = await fetchAuthToken();
  const authHeaders = {
    "User-Agent": UA,
    Accept: "application/json",
    Referer: REFERER,
    Authorization: `Bearer ${jwt}`,
    "x-fce-client": "web-client",
  };

  const listResp = await fetchWithTimeout(
    `${SITE_URL}/api/public-mailbox?fullMailboxId=${encodeURIComponent(addr)}`,
    { method: "GET", headers: authHeaders },
  );
  if (!listResp.ok) {
    throw new Error(`freecustom: 获取邮件失败 ${listResp.status}`);
  }
  const listData = (await listResp.json()) as FreeListResponse;
  if (!listData.success || !Array.isArray(listData.data)) {
    return [];
  }

  const emails: Email[] = [];
  for (const item of listData.data) {
    if (!item.id) continue;
    let full: FreeMessage = item;
    try {
      const msgResp = await fetchWithTimeout(
        `${SITE_URL}/api/public-mailbox?fullMailboxId=${encodeURIComponent(addr)}&messageId=${encodeURIComponent(String(item.id))}`,
        { method: "GET", headers: authHeaders },
      );
      if (msgResp.ok) {
        const msgData = (await msgResp.json()) as {
          success?: boolean;
          data?: FreeMessage;
        };
        if (msgData.success && msgData.data) {
          full = msgData.data;
        }
      }
    } catch {
      // 正文补全失败时退回列表元数据
    }

    emails.push(
      normalizeEmail(
        {
          id: String(full.id ?? ""),
          from: full.from || "",
          to: full.to || addr,
          subject: full.subject || "",
          text: full.text || "",
          html: full.html || "",
          date: full.date || "",
          isRead: false,
          attachments: [],
        },
        addr,
      ),
    );
  }

  return emails;
}

type FreeDomain = {
  domain: string;
  tier?: string;
  tags?: string[];
  expires_at?: string;
  expires_in_days?: number;
  expiring_soon?: boolean;
};

type FreeMessage = {
  id?: string | number;
  from?: string;
  to?: string;
  subject?: string;
  date?: string;
  html?: string;
  text?: string;
  hasAttachment?: boolean;
  wasAttachmentStripped?: boolean;
  otp?: string | null;
  verificationLink?: string | null;
};

type FreeListResponse = {
  success?: boolean;
  message?: string;
  data?: FreeMessage[];
  encryptedMailbox?: string;
};
