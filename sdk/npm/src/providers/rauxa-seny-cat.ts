import { InternalEmailInfo, Email, Channel } from "../types";
import * as mailinator from "./mailinator";
const CHANNEL: Channel = "rauxa-seny-cat";
const DOMAIN = "rauxa.seny.cat";
function randomString(length: number): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  let result = "";
  for (let i = 0; i < length; i++) {
    result += chars[Math.floor(Math.random() * chars.length)];
  }
  return result;
}
export async function generateEmail(): Promise<InternalEmailInfo> {
  return { channel: CHANNEL, email: `${randomString(12)}@${DOMAIN}` };
}
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  return mailinator.getEmails(token, email);
}
