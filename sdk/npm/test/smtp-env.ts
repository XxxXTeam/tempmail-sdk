import nodemailer from "nodemailer";

export type SmtpEnv = {
  host: string;
  port: number;
  secure: boolean;
  user: string;
  pass: string;
  from: string;
  timeout: number;
};

export function getSmtpEnv(): SmtpEnv | null {
  const host = process.env.SMTP_HOST?.trim();
  const user = process.env.SMTP_USER?.trim();
  const pass = process.env.SMTP_PASS?.trim();
  const from = process.env.SMTP_FROM?.trim() || user || "";
  if (!host || !user || !pass || !from) return null;
  const port = Number(process.env.SMTP_PORT || 465);
  const secureRaw = process.env.SMTP_SECURE?.trim().toLowerCase();
  const secure = secureRaw
    ? secureRaw === "1" || secureRaw === "true"
    : port === 465;
  const timeout = Number(process.env.SMTP_TIMEOUT || 15000);
  return { host, port, secure, user, pass, from, timeout };
}

export async function sendSmtp(
  to: string,
  subject: string,
  text: string,
  html: string,
): Promise<boolean> {
  const smtp = getSmtpEnv();
  if (!smtp) {
    console.log("    SMTP 环境变量未配置，跳过发送");
    return false;
  }
  const transport = nodemailer.createTransport({
    host: smtp.host,
    port: smtp.port,
    secure: smtp.secure,
    auth: { user: smtp.user, pass: smtp.pass },
    connectionTimeout: smtp.timeout,
  });
  await transport.sendMail({ from: smtp.from, to, subject, text, html });
  return true;
}
