import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "24mail-chacuo";
const BASE_URL = "http://24mail.chacuo.net";
const DOMAINS = ["chacuo.net", "027168.com"];
const HEADERS: Record<string, string> = {
  Accept: "application/json, text/javascript, */*; q=0.01",
  "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
  "Content-Type": "application/x-www-form-urlencoded; charset=UTF-8",
  Origin: BASE_URL,
  Referer: `${BASE_URL}/`,
  "User-Agent":
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
  "x-requested-with": "XMLHttpRequest",
};

function randomString(n: number): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  let result = "";
  for (let i = 0; i < n; i++) {
    result += chars[Math.floor(Math.random() * chars.length)];
  }
  return result;
}

/**
 * 创建 24mail.chacuo.net 临时邮箱
 * API: POST / → data={name}%40{domain}&type=refresh&arg=
 * 返回 JSON {"status":1,"info":"ok","data":[...]}
 * 无需额外 token，邮箱地址即为凭证
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const name = randomString(10);
  const domain = DOMAINS[Math.floor(Math.random() * DOMAINS.length)];
  const email = `${name}@${domain}`;

  const body = `data=${encodeURIComponent(email)}&type=refresh&arg=`;
  const response = await fetchWithTimeout(`${BASE_URL}/`, {
    method: "POST",
    headers: HEADERS,
    body,
  });

  if (!response.ok) {
    throw new Error(`24mail-chacuo: 创建邮箱失败 ${response.status}`);
  }

  const data = (await response.json()) as { status: number; info: string };
  if (data.status !== 1) {
    throw new Error(
      `24mail-chacuo: 创建失败 status=${data.status} info=${data.info}`,
    );
  }

  return {
    channel: CHANNEL,
    email,
    token: email,
  };
}

/**
 * 获取 24mail.chacuo.net 邮件列表
 * API: POST / → data={email}&type=refresh&arg=
 * 返回 JSON {"status":1,"data":[{"list":[{MID,FROM,SUBJECT,CONTENT,TIME}]}]}
 * @param token - 邮箱地址
 * @param email - 邮箱地址
 */
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  const addr = token || email;
  if (!addr) {
    throw new Error("24mail-chacuo: 缺少邮箱地址");
  }

  const body = `data=${encodeURIComponent(addr)}&type=refresh&arg=`;
  const response = await fetchWithTimeout(`${BASE_URL}/`, {
    method: "POST",
    headers: HEADERS,
    body,
  });

  if (!response.ok) {
    throw new Error(`24mail-chacuo: 获取邮件失败 ${response.status}`);
  }

  const result = (await response.json()) as ChacuoResponse;
  if (
    result.status !== 1 ||
    !Array.isArray(result.data) ||
    result.data.length === 0
  ) {
    return [];
  }

  const list = result.data[0]?.list;
  if (!Array.isArray(list) || list.length === 0) {
    return [];
  }

  return list.map((msg) =>
    normalizeEmail(
      {
        id: msg.MID || "",
        from: msg.FROM || "",
        to: addr,
        subject: msg.SUBJECT || "",
        text: msg.CONTENT || "",
        html: msg.CONTENT || "",
        date: msg.TIME || "",
        isRead: false,
        attachments: [],
      },
      addr,
    ),
  );
}

type ChacuoResponse = {
  status: number;
  info?: string;
  data?: Array<{
    list?: ChacuoMessage[];
  }>;
};

type ChacuoMessage = {
  MID?: string;
  FROM?: string;
  SUBJECT?: string;
  CONTENT?: string;
  TIME?: string;
};
