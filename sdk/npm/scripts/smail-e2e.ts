/**
 * smail-pw 端到端：生成邮箱 → Ethereal SMTP 发信 → 轮询解析
 */
import nodemailer from 'nodemailer';
import * as smail from '../src/providers/smail-pw';
import { fetchWithTimeout } from '../src/retry';

const POLL_MS = 4000;
const MAX_ROUNDS = 30;

async function sendViaEthereal(to: string, marker: string): Promise<void> {
  const testAccount = await nodemailer.createTestAccount();
  const transport = nodemailer.createTransport({
    host: 'smtp.ethereal.email',
    port: 587,
    secure: false,
    auth: { user: testAccount.user, pass: testAccount.pass },
  });
  const info = await transport.sendMail({
    from: `"SDK Probe" <${testAccount.user}>`,
    to,
    subject: `smail-sdk ${marker}`,
    text: `plain ${marker}`,
    html: `<p>html <b>${marker}</b></p>`,
  });
  console.log('Ethereal 已投递 messageId=', info.messageId);
}

async function fetchRaw(token: string): Promise<string> {
  const res = await fetchWithTimeout('https://smail.pw/_root.data', {
    headers: { referer: 'https://smail.pw/', Cookie: token },
  });
  return res.text();
}

async function main(): Promise<void> {
  const marker = `e2e-${Date.now()}`;
  const mailbox = await smail.generateEmail();
  console.log('邮箱:', mailbox.email);
  console.log('探针:', marker);
  console.log('正在经 Ethereal SMTP 发信...\n');
  await sendViaEthereal(mailbox.email, marker);

  for (let i = 1; i <= MAX_ROUNDS; i++) {
    const raw = await fetchRaw(mailbox.token);
    const emails = await smail.getEmails(mailbox.token, mailbox.email);
    const nonEmptyEmails = !/"emails",\[\]/.test(raw) && raw.includes('"emails"');
    console.log(
      `[${i}/${MAX_ROUNDS}] parsed=${emails.length} raw_has_emails=${nonEmptyEmails} text_len=${emails[0]?.text?.length ?? 0}`,
    );
    if (emails.length > 0) {
      console.log('\n成功解析邮件:');
      console.log(JSON.stringify(emails[0], null, 2));
      if (!emails[0].text && !emails[0].html) {
        console.log('\n⚠ 主题/发件人已解析，但正文为空（需 /api/email/:id）');
      }
      return;
    }
    if (nonEmptyEmails) {
      const idx = raw.indexOf('"emails"');
      console.log('--- raw 片段 ---\n', raw.slice(idx, idx + 500));
    }
    await new Promise((r) => setTimeout(r, POLL_MS));
  }
  console.log('\n超时。最后 raw 前 800 字符:\n', (await fetchRaw(mailbox.token)).slice(0, 800));
  process.exit(1);
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
