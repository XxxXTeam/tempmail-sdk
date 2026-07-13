import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "mailmomy";
const BASE_URL = "https://mailmomy.com";

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
 * 拉取 mailmomy 当前可用域名池。
 * GET /api/domains/active → JSON string array
 * 优先选包含 "mailmomy" 或 ".com" 的域名（较稳定），全池均可用。
 */
async function pickDomain(): Promise<string> {
  const resp = await fetchWithTimeout(`${BASE_URL}/api/domains/active`, {
    method: "GET",
    headers: { "User-Agent": UA, Accept: "application/json" },
  });
  if (!resp.ok) {
    /* 域名接口失败时回退到默认 mailmomy.com */
    return "mailmomy.com";
  }
  const data = (await resp.json()) as unknown;
  if (!Array.isArray(data) || data.length === 0) {
    return "mailmomy.com";
  }
  const list = data.filter(
    (d): d is string => typeof d === "string" && d.length > 0,
  );
  if (list.length === 0) return "mailmomy.com";
  return list[Math.floor(Math.random() * list.length)];
}

/**
 * 创建 mailmomy.com 临时邮箱
 * 服务免注册，直接构造 <local>@<域名> 即可收信
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
 * 获取 mailmomy.com 邮件列表
 * GET /api/mail/messages?to=<email>&page=1&limit=20
 * 返回 {emails:[{id,recipient,from,subject,message,bodyText,receivedAt}], total, page, limit, pages}
 */
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  const addr = token || email;
  if (!addr) {
    throw new Error("mailmomy: 缺少邮箱地址");
  }

  const url = `${BASE_URL}/api/mail/messages?to=${encodeURIComponent(addr)}&page=1&limit=20`;
  const resp = await fetchWithTimeout(url, {
    method: "GET",
    headers: { "User-Agent": UA, Accept: "application/json" },
  });

  if (!resp.ok) {
    throw new Error(`mailmomy: 获取邮件失败 ${resp.status}`);
  }

  const data = (await resp.json()) as MailmomyListResponse;
  if (!Array.isArray(data.emails)) {
    return [];
  }

  return data.emails.map((msg) =>
    normalizeEmail(
      {
        id: String(msg.id ?? ""),
        from: msg.from || "",
        to: msg.recipient || addr,
        subject: msg.subject || "",
        text: msg.bodyText || msg.message || "",
        html: msg.message || "",
        date: msg.receivedAt || "",
        isRead: false,
        attachments: [],
      },
      addr,
    ),
  );
}

type MailmomyMessage = {
  id?: string;
  recipient?: string;
  from?: string;
  subject?: string;
  message?: string;
  bodyText?: string;
  receivedAt?: string;
};

type MailmomyListResponse = {
  emails?: MailmomyMessage[];
  total?: number;
  page?: number;
  limit?: number;
  pages?: number;
};
