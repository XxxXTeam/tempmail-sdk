import { generateEmail, getEmails, TempEmailClient, Channel } from '../src';
import nodemailer from 'nodemailer';

type SmtpConfig = {
  host: string;
  port: number;
  user?: string;
  pass?: string;
  from: string;
  secure: boolean;
  timeoutMs: number;
  subject: string;
  body: string;
};

type PollConfig = {
  intervalMs: number;
  max: number;
};

function parseBoolean(value?: string): boolean {
  if (!value) return false;
  return value === '1' || value.toLowerCase() === 'true';
}

function parsePositiveNumber(value: string | undefined, fallback: number): number {
  if (!value) return fallback;
  const parsed = Number(value);
  if (!Number.isFinite(parsed) || parsed <= 0) return fallback;
  return parsed;
}

function getSmtpConfig(): SmtpConfig | null {
  const enabled = parseBoolean(process.env.TEMPMAIL_SMTP_ENABLED);
  if (!enabled) return null;

  const host = process.env.TEMPMAIL_SMTP_HOST?.trim();
  const portRaw = process.env.TEMPMAIL_SMTP_PORT?.trim();
  const port = portRaw ? Number(portRaw) : 0;

  if (!host || !port || !Number.isFinite(port)) {
    console.warn('SMTP 已启用，但 TEMPMAIL_SMTP_HOST/TEMPMAIL_SMTP_PORT 未正确配置，跳过发送。');
    return null;
  }

  const user = process.env.TEMPMAIL_SMTP_USER?.trim();
  const pass = process.env.TEMPMAIL_SMTP_PASS?.trim();
  const from = (process.env.TEMPMAIL_SMTP_FROM?.trim() || user || '').trim();

  if (!from) {
    console.warn('SMTP 已启用，但 TEMPMAIL_SMTP_FROM 未配置且 USER 为空，跳过发送。');
    return null;
  }

  const secureEnv = process.env.TEMPMAIL_SMTP_SECURE?.trim();
  const secure = secureEnv ? parseBoolean(secureEnv) : port === 465;

  const timeoutMs = parsePositiveNumber(process.env.TEMPMAIL_SMTP_TIMEOUT, 10000);
  const subject = process.env.TEMPMAIL_SMTP_SUBJECT?.trim() || 'TempMail SDK SMTP Test';
  const body = process.env.TEMPMAIL_SMTP_BODY?.trim() || 'This is a tempmail SDK SMTP test email.';

  return {
    host,
    port,
    user,
    pass,
    from,
    secure,
    timeoutMs,
    subject,
    body,
  };
}

function getPollConfig(): PollConfig {
  return {
    intervalMs: parsePositiveNumber(process.env.TEMPMAIL_POLL_INTERVAL_MS, 3000),
    max: parsePositiveNumber(process.env.TEMPMAIL_POLL_MAX, 10),
  };
}

function createTraceId(): string {
  return `${Date.now()}-${Math.random().toString(36).slice(2, 8)}`;
}

function sleep(ms: number) {
  return new Promise<void>((resolve) => setTimeout(resolve, ms));
}

async function sendSmtpTestEmail(targetEmail: string): Promise<{ marker: string } | null> {
  const smtp = getSmtpConfig();
  if (!smtp) return null;

  const marker = `tempmail-sdk:${createTraceId()}`;
  const subject = `${smtp.subject} [${marker}]`;
  const text = `${smtp.body}\n\n[${marker}]`;
  const html = `${smtp.body}<br/><br/><strong>[${marker}]</strong>`;

  const transport = nodemailer.createTransport({
    host: smtp.host,
    port: smtp.port,
    secure: smtp.secure,
    auth: smtp.user && smtp.pass ? { user: smtp.user, pass: smtp.pass } : undefined,
    connectionTimeout: smtp.timeoutMs,
  });

  try {
    await transport.sendMail({
      from: smtp.from,
      to: targetEmail,
      subject,
      text,
      html,
    });
    console.log(`SMTP 测试邮件已发送到 ${targetEmail}`);
    return { marker };
  } catch (error) {
    console.warn(`SMTP 发送失败：${error}`);
    return null;
  }
}

function emailHasMarker(content: string | undefined, marker: string): boolean {
  return !!content && content.includes(marker);
}

async function pollForEmail(client: TempEmailClient, marker: string): Promise<boolean> {
  const { intervalMs, max } = getPollConfig();

  for (let attempt = 1; attempt <= max; attempt++) {
    try {
      const result = await client.getEmails();
      const found = result.emails.some((email) => {
        return (
          emailHasMarker(email.subject, marker) ||
          emailHasMarker(email.text, marker) ||
          emailHasMarker(email.html, marker)
        );
      });
      if (found) {
        console.log(`轮询成功：已收到包含标识 ${marker} 的邮件。`);
        return true;
      }
    } catch (error) {
      console.warn(`轮询第 ${attempt} 次失败：${error}`);
    }

    if (attempt < max) {
      await sleep(intervalMs);
    }
  }

  console.warn(`轮询超时：未收到包含标识 ${marker} 的邮件。`);
  return false;
}

async function testGenerateEmail() {
  console.log('=== Test Generate Email ===\n');

  // Test each channel（与 src/index.ts allChannels 一致）
  const channels: Channel[] = [
    'tempmail',
    'linshi-email',
    'tempmail-lol',
    'chatgpt-org-uk',
    'temp-mail-io',
    'awamail',
    'mail-tm',
    'dropmail',
    'guerrillamail',
    'maildrop',
    'smail-pw',
  ];

  for (const channel of channels) {
    try {
      console.log(`Testing channel: ${channel}`);
      const emailInfo = await generateEmail({ channel });
      if (!emailInfo) {
        console.log('  Failed to generate email');
        console.log();
        continue;
      }
      console.log(`  Channel: ${emailInfo.channel}`);
      console.log(`  Email: ${emailInfo.email}`);
      if (emailInfo.expiresAt) console.log(`  Expires: ${emailInfo.expiresAt}`);
      if (emailInfo.createdAt) console.log(`  Created: ${emailInfo.createdAt}`);
      console.log();
    } catch (error) {
      console.error(`  Error: ${error}`);
      console.log();
    }
  }
}

async function testGetEmails() {
  console.log('=== Test Get Emails ===\n');

  // Generate and check emails using client
  const client = new TempEmailClient();
  
  try {
    const emailInfo = await client.generate({ channel: 'tempmail' });
    if (!emailInfo) {
      console.log('Generated: null');
      return;
    }
    console.log(`Generated: ${emailInfo.channel} - ${emailInfo.email}`);
    
    const result = await client.getEmails();
    console.log(`Emails count: ${result.emails.length}`);

    // 展示标准化的邮件格式
    for (const email of result.emails) {
      console.log(`  ID: ${email.id}`);
      console.log(`  From: ${email.from}`);
      console.log(`  To: ${email.to}`);
      console.log(`  Subject: ${email.subject}`);
      console.log(`  Date: ${email.date}`);
      console.log(`  IsRead: ${email.isRead}`);
      console.log(`  Text: ${email.text.substring(0, 100)}`);
      console.log(`  HTML: ${email.html.substring(0, 100)}`);
      console.log(`  Attachments: ${email.attachments.length}`);
      console.log();
    }
    console.log();

    const smtpResult = await sendSmtpTestEmail(emailInfo.email);
    if (smtpResult) {
      await pollForEmail(client, smtpResult.marker);
    }
  } catch (error) {
    console.error(`Error: ${error}`);
  }
}

async function main() {
  await testGenerateEmail();
  await testGetEmails();
}

main().catch(console.error);
