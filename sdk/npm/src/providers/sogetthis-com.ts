import { InternalEmailInfo, Email, Channel } from "../types";
import * as mailinator from "./mailinator";

const CHANNEL: Channel = "sogetthis-com";
const DOMAIN = "sogetthis.com";

function randomString(length: number): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  let result = "";
  for (let i = 0; i < length; i++) {
    result += chars[Math.floor(Math.random() * chars.length)];
  }
  return result;
}

/**
 * 创建 sogetthis.com 临时邮箱（mailinator 官方姊妹域名，MX 指向 mail.mailinator.com）
 * 无状态生成，直接构造随机 local part @ sogetthis.com
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const local = randomString(12);
  return {
    channel: CHANNEL,
    email: `${local}@${DOMAIN}`,
  };
}

/**
 * 获取 sogetthis.com 邮件列表
 * 直接复用 mailinator 的公开 API：GET /api/v2/domains/public/inboxes/{inbox}
 * mailinator 后端把所有姊妹域名的邮件按 inbox 名聚合到 public 域
 */
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  return mailinator.getEmails(token, email);
}
