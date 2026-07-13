import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "nimail";
const BASE_URL = "https://www.nimail.cn";

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
 * 创建 nimail.cn 临时邮箱
 * POST /api/applymail → 注册邮箱（前缀自选）
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const name = randomLocal(10);
  const email = `${name}@nimail.cn`;

  const resp = await fetchWithTimeout(`${BASE_URL}/api/applymail`, {
    method: "POST",
    headers: {
      "User-Agent": UA,
      "Content-Type": "application/x-www-form-urlencoded",
      Origin: BASE_URL,
      Referer: `${BASE_URL}/`,
    },
    body: `mail=${encodeURIComponent(email)}`,
  });

  if (!resp.ok) {
    throw new Error(`nimail: 创建邮箱失败 ${resp.status}`);
  }

  const data = (await resp.json()) as NimailApplyResponse;
  if (data.success !== "true" || !data.user) {
    throw new Error(`nimail: 创建邮箱失败 ${JSON.stringify(data)}`);
  }

  return {
    channel: CHANNEL,
    email: data.user,
    token: data.user,
  };
}

/**
 * 获取 nimail.cn 邮件列表
 * POST /api/getmails → 获取当前邮箱的消息
 */
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  const addr = token || email;
  if (!addr) {
    throw new Error("nimail: 缺少邮箱地址");
  }

  const resp = await fetchWithTimeout(`${BASE_URL}/api/getmails`, {
    method: "POST",
    headers: {
      "User-Agent": UA,
      "Content-Type": "application/x-www-form-urlencoded",
      Origin: BASE_URL,
      Referer: `${BASE_URL}/`,
    },
    body: `mail=${encodeURIComponent(addr)}&time=0`,
  });

  if (!resp.ok) {
    throw new Error(`nimail: 获取邮件失败 ${resp.status}`);
  }

  const data = (await resp.json()) as NimailGetResponse;
  if (data.success !== "true" || !Array.isArray(data.mail)) {
    return [];
  }

  return data.mail.map((msg) =>
    normalizeEmail(
      {
        id: String(msg.id ?? msg.time ?? ""),
        from: msg.from || msg.sender || "",
        to: addr,
        subject: msg.subject || msg.title || "",
        text: msg.text || msg.content || "",
        html: msg.html || msg.content || "",
        date: msg.date || msg.time || "",
        isRead: false,
        attachments: [],
      },
      addr,
    ),
  );
}

type NimailApplyResponse = {
  delay?: string;
  tips?: string;
  user?: string;
  success?: string;
  time?: number;
};

type NimailGetResponse = {
  to?: string;
  mail?: NimailMessage[];
  success?: string;
  time?: number;
};

type NimailMessage = {
  id?: string | number;
  from?: string;
  sender?: string;
  subject?: string;
  title?: string;
  text?: string;
  html?: string;
  content?: string;
  date?: string;
  time?: string | number;
};
