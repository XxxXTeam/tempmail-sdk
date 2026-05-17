import { generateEmail, getEmails, TempEmailClient, Channel, setConfig } from '../src';
import nodemailer from 'nodemailer';

setConfig({ telemetryEnabled: false });

const SMTP_HOST = 'smtp.exmail.qq.com';
const SMTP_PORT = 465;
const SMTP_USER = 'supper@openel.top';
const SMTP_PASS = 'PKZT5rgvUvGdgcxe';

function sleep(ms: number) { return new Promise<void>(r => setTimeout(r, ms)); }

async function sendEmail(to: string, marker: string): Promise<boolean> {
  const transport = nodemailer.createTransport({
    host: SMTP_HOST, port: SMTP_PORT, secure: true,
    auth: { user: SMTP_USER, pass: SMTP_PASS }, connectionTimeout: 15000,
  });
  try {
    await transport.sendMail({
      from: SMTP_USER, to,
      subject: `Fix Test [${marker}]`,
      text: `正文测试 marker: ${marker}`,
      html: `<h1>Fix Test</h1><p>正文测试 marker: <b>${marker}</b></p>`,
    });
    return true;
  } catch (e: any) { console.log(`  SMTP失败: ${e.message}`); return false; }
}

async function testFix(channel: Channel) {
  console.log(`\n=== 测试 ${channel} ===`);
  const client = new TempEmailClient();
  const info = await client.generate({ channel, channelFallback: false });
  if (!info) { console.log('  ❌ 生成失败'); return; }
  console.log(`  邮箱: ${info.email}`);

  const marker = `fix-${channel}-${Date.now()}`;
  const sent = await sendEmail(info.email, marker);
  if (!sent) return;
  console.log('  SMTP 已发送, 等待...');

  for (let i = 1; i <= 10; i++) {
    await sleep(5000);
    const r = await client.getEmails();
    if (!r.success) { console.log(`  轮询 ${i}: success=false`); continue; }
    const found = r.emails.find(e => e.subject?.includes(marker) || e.text?.includes(marker) || e.html?.includes(marker));
    if (found) {
      console.log(`  ✅ 收到邮件!`);
      console.log(`    Subject: ${found.subject}`);
      console.log(`    Text长度: ${(found.text || '').length}`);
      console.log(`    HTML长度: ${(found.html || '').length}`);
      console.log(`    Text预览: ${(found.text || '').substring(0, 80)}`);
      console.log(`    HTML预览: ${(found.html || '').substring(0, 80)}`);
      return;
    }
    console.log(`  轮询 ${i}: ${r.emails.length} 封, 未匹配`);
  }
  console.log('  ⚠️ 超时');
}

async function main() {
  const channels: Channel[] = ['moakt', '10minute-one', 'guerrillamail'];
  for (const ch of channels) {
    await testFix(ch);
  }
}

main().catch(console.error);
