import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "restmail-net";
const BASE_URL = "https://restmail.net";
const DOMAIN = "restmail.net";

/**
 * 生成随机用户名（10-12 位字母数字）
 */
function randomUsername(): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  const len = 10 + Math.floor(Math.random() * 3); // 10-12 位
  let result = "";
  for (let i = 0; i < len; i++) {
    result += chars[Math.floor(Math.random() * chars.length)];
  }
  return result;
}

/**
 * 创建 restmail.net 临时邮箱
 * Mozilla 开源项目，公共收件箱模式，随机生成用户名即可收信，无需注册
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const user = randomUsername();
  const email = `${user}@${DOMAIN}`;
  return {
    channel: CHANNEL,
    email,
    token: "",
  };
}

/**
 * 获取 restmail.net 邮件列表
 * GET /mail/{username} 返回 JSON 数组
 * @param _token - 不使用（空字符串）
 * @param email - 邮箱地址
 */
export async function getEmails(_token: string, email: string): Promise<Email[]> {
  const address = (email || "").trim();
  if (!address) throw new Error("restmail-net: 缺少邮箱地址");

  /* 从邮箱地址中提取 username */
  const atIdx = address.indexOf("@");
  const username = atIdx > 0 ? address.substring(0, atIdx) : address;

  /* 获取邮件列表 */
  const url = `${BASE_URL}/mail/${username}`;
  const response = await fetchWithTimeout(url, {
    headers: { Accept: "application/json" },
  });
  if (!response.ok) {
    throw new Error(`restmail-net: 获取邮件列表失败 http ${response.status}`);
  }

  const messages: any[] = await response.json();
  if (!Array.isArray(messages) || messages.length === 0) {
    return [];
  }

  return messages.map((msg) => normalizeEmail(flattenMessage(msg, address), address));
}

/**
 * 将 restmail.net 原始消息结构展平为 normalizeEmail 可处理的对象
 * from: 优先取 from[0].address，否则取 headers.from
 * subject: 取 subject 字段
 * body: 优先 html，其次 text
 * date: 取 receivedAt
 */
function flattenMessage(msg: any, recipient: string): Record<string, unknown> {
  const flat: Record<string, unknown> = {
    to: recipient,
  };

  /* 提取 from */
  if (Array.isArray(msg.from) && msg.from.length > 0 && msg.from[0].address) {
    flat.from = msg.from[0].address;
  } else if (msg.headers && msg.headers.from) {
    flat.from = msg.headers.from;
  }

  /* 提取 subject */
  if (msg.subject) {
    flat.subject = msg.subject;
  }

  /* 提取 body：优先 html，其次 text */
  if (msg.html) {
    flat.html = msg.html;
  }
  if (msg.text) {
    flat.text = msg.text;
  }

  /* 提取日期 */
  if (msg.receivedAt) {
    flat.date = msg.receivedAt;
  }

  return flat;
}
