import { InternalEmailInfo, Email, Channel } from "../types";
import * as mailinator from "./mailinator";

const CHANNEL: Channel = "mail-fsmash-org";
const DOMAIN = "mail.fsmash.org";

function randomString(length: number): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  let result = "";
  for (let i = 0; i < length; i++) {
    result += chars[Math.floor(Math.random() * chars.length)];
  }
  return result;
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const local = randomString(12);
  return { channel: CHANNEL, email: `${local}@${DOMAIN}` };
}

export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  return mailinator.getEmails(token, email);
}
