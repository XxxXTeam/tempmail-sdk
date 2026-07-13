import { InternalEmailInfo, Email, Channel } from "../types";
import * as mailinator from "./mailinator";
const CHANNEL: Channel = "j-fairuse-org";
const DOMAIN = "j.fairuse.org";
function randomString(l: number): string {
  const c = "abcdefghijklmnopqrstuvwxyz0123456789";
  let r = "";
  for (let i = 0; i < l; i++) r += c[Math.floor(Math.random() * c.length)];
  return r;
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
