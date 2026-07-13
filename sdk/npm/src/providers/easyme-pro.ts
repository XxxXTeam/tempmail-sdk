import { InternalEmailInfo, Email, Channel } from "../types";
import * as mailmomy from "./mailmomy";
const CHANNEL: Channel = "easyme-pro";
const DOMAIN = "easyme.pro";
function randomString(length: number): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  let result = "";
  for (let i = 0; i < length; i++) {
    result += chars[Math.floor(Math.random() * chars.length)];
  }
  return result;
}
export async function generateEmail(): Promise<InternalEmailInfo> {
  return {
    channel: CHANNEL,
    email: `${randomString(10)}@${DOMAIN}`,
    token: `${randomString(10)}@${DOMAIN}`,
  };
}
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  return mailmomy.getEmails(token, email);
}
