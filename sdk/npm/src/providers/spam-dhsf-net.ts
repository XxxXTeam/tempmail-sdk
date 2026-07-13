import { InternalEmailInfo, Email, Channel } from "../types";
import * as mailinator from "./mailinator";

const CHANNEL: Channel = "spam-dhsf-net";
const DOMAIN = "spam.dhsf.net";

function randomString(length: number): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  let result = "";
  for (let i = 0; i < length; i++) {
    result += chars[Math.floor(Math.random() * chars.length)];
  }
  return result;
}

/**
 * 创建 spam.dhsf.net 临时邮箱（mailinator 姊妹子域名，MX 指向 mail.mailinator.com）
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const local = randomString(12);
  return { channel: CHANNEL, email: `${local}@${DOMAIN}` };
}

/**
 * 获取邮件列表（复用 mailinator public API）
 */
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  return mailinator.getEmails(token, email);
}
