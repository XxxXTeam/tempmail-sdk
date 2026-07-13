import { InternalEmailInfo, Email, Channel } from "../types";
import * as mailinator from "./mailinator";
const CHANNEL: Channel = "mailinatorzz-mooo-com";
export async function generateEmail(): Promise<InternalEmailInfo> {
  const c = "abcdefghijklmnopqrstuvwxyz0123456789";
  let r = "";
  for (let i = 0; i < 12; i++) r += c[Math.floor(Math.random() * c.length)];
  return { channel: CHANNEL, email: `${r}@mailinatorzz.mooo.com` };
}
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  return mailinator.getEmails(token, email);
}
