import { generateEmail, getEmails, TempEmailClient, Channel, setConfig } from '../src';
import nodemailer from 'nodemailer';

// SMTP 配置
const SMTP_HOST = 'smtp.exmail.qq.com';
const SMTP_PORT = 465;
const SMTP_USER = 'supper@openel.top';
const SMTP_PASS = 'PKZT5rgvUvGdgcxe';
const SMTP_FROM = 'supper@openel.top';
const SMTP_SECURE = true;

// 所有 28 个渠道
const allChannels: Channel[] = [
  'tempmail', 'tempmail-cn', 'tmpmails', 'tempmailg', 'ta-easy',
  '10mail-wangtz', '10minute-one', 'linshi-email', 'linshiyou', 'mffac',
  'tempmail-lol', 'chatgpt-org-uk', 'temp-mail-io', 'awamail', 'temporary-email-org',
  'mail-tm', 'mail-cx', 'dropmail', 'guerrillamail', 'maildrop',
  'smail-pw', 'boomlify', 'minmail', 'vip-215', 'anonbox',
  'fake-legal', 'moakt', 'etempmail',
];

interface ChannelResult {
  channel: string;
  generateOk: boolean;
  generateErr: string;
  email: string;
  getEmailsOk: boolean;
  getEmailsErr: string;
  smtpSent: boolean;
  smtpReceived: boolean;
  timeMs: number;
}

function sleep(ms: number) {
  return new Promise<void>((r) => setTimeout(r, ms));
}

function createTraceId(): string {
  return `${Date.now()}-${Math.random().toString(36).slice(2, 8)}`;
}

async function sendSmtpEmail(to: string, marker: string): Promise<boolean> {
  const transport = nodemailer.createTransport({
    host: SMTP_HOST,
    port: SMTP_PORT,
    secure: SMTP_SECURE,
    auth: { user: SMTP_USER, pass: SMTP_PASS },
    connectionTimeout: 15000,
  });

  try {
    await transport.sendMail({
      from: SMTP_FROM,
      to,
      subject: `SDK Test [${marker}]`,
      text: `Test email marker: ${marker}`,
      html: `<p>Test email marker: <b>${marker}</b></p>`,
    });
    return true;
  } catch (e: any) {
    console.log(`    SMTP 发送失败: ${e.message}`);
    return false;
  }
}

async function pollForMarker(client: TempEmailClient, marker: string, maxAttempts = 8, intervalMs = 5000): Promise<boolean> {
  for (let i = 1; i <= maxAttempts; i++) {
    try {
      const result = await client.getEmails();
      if (result.success) {
        const found = result.emails.some(
          (e) => e.subject?.includes(marker) || e.text?.includes(marker) || e.html?.includes(marker)
        );
        if (found) return true;
      }
    } catch {}
    if (i < maxAttempts) await sleep(intervalMs);
  }
  return false;
}

async function testChannel(channel: Channel): Promise<ChannelResult> {
  const result: ChannelResult = {
    channel,
    generateOk: false,
    generateErr: '',
    email: '',
    getEmailsOk: false,
    getEmailsErr: '',
    smtpSent: false,
    smtpReceived: false,
    timeMs: 0,
  };

  const start = Date.now();
  console.log(`\n[${'='.repeat(50)}]`);
  console.log(`测试渠道: ${channel}`);

  // 1. 测试 generateEmail
  try {
    const client = new TempEmailClient();
    const info = await client.generate({ channel, channelFallback: false });
    if (!info) {
      result.generateErr = '返回 null';
      console.log(`  ❌ 生成邮箱失败: 返回 null`);
      result.timeMs = Date.now() - start;
      return result;
    }
    result.generateOk = true;
    result.email = info.email;
    console.log(`  ✅ 生成邮箱: ${info.email}`);

    // 2. 测试 getEmails (无邮件时也算成功)
    try {
      const emailResult = await client.getEmails();
      result.getEmailsOk = emailResult.success;
      if (emailResult.success) {
        console.log(`  ✅ 获取邮件成功 (${emailResult.emails.length} 封)`);
      } else {
        result.getEmailsErr = '返回 success: false';
        console.log(`  ⚠️ 获取邮件返回 success: false`);
      }
    } catch (e: any) {
      result.getEmailsErr = e.message || String(e);
      console.log(`  ❌ 获取邮件异常: ${result.getEmailsErr}`);
    }

    // 3. 发送 SMTP 测试邮件并轮询
    const marker = `test-${channel}-${createTraceId()}`;
    console.log(`  📧 发送 SMTP 测试邮件...`);
    result.smtpSent = await sendSmtpEmail(info.email, marker);
    if (result.smtpSent) {
      console.log(`  ✅ SMTP 发送成功，开始轮询收件箱...`);
      result.smtpReceived = await pollForMarker(client, marker);
      if (result.smtpReceived) {
        console.log(`  ✅ 成功收到 SMTP 测试邮件`);
      } else {
        console.log(`  ⚠️ 轮询超时，未收到 SMTP 测试邮件`);
      }
    }
  } catch (e: any) {
    result.generateErr = e.message || String(e);
    console.log(`  ❌ 生成邮箱异常: ${result.generateErr}`);
  }

  result.timeMs = Date.now() - start;
  console.log(`  ⏱️ 耗时: ${(result.timeMs / 1000).toFixed(1)}s`);
  return result;
}

async function main() {
  // 关闭遥测，避免干扰测试
  setConfig({ telemetryEnabled: false });

  console.log('========================================');
  console.log('  TempMail SDK 全渠道测试');
  console.log(`  测试时间: ${new Date().toISOString()}`);
  console.log(`  渠道总数: ${allChannels.length}`);
  console.log('========================================');

  const results: ChannelResult[] = [];

  for (const ch of allChannels) {
    const r = await testChannel(ch);
    results.push(r);
  }

  // 汇总报告
  console.log('\n\n');
  console.log('╔══════════════════════════════════════════════════════════════════════════╗');
  console.log('║                         全渠道测试汇总报告                               ║');
  console.log('╠══════════════════════════════════════════════════════════════════════════╣');

  const genOk = results.filter((r) => r.generateOk);
  const genFail = results.filter((r) => !r.generateOk);
  const getOk = results.filter((r) => r.getEmailsOk);
  const smtpOk = results.filter((r) => r.smtpReceived);

  console.log(`\n  📊 统计:`);
  console.log(`     生成邮箱成功: ${genOk.length}/${results.length}`);
  console.log(`     获取邮件成功: ${getOk.length}/${results.length}`);
  console.log(`     SMTP 收信成功: ${smtpOk.length}/${genOk.length} (仅统计生成成功的渠道)`);

  // 成功的渠道
  if (genOk.length > 0) {
    console.log(`\n  ✅ 可用渠道 (${genOk.length}):`);
    for (const r of genOk) {
      const flags = [
        r.getEmailsOk ? 'getEmails✅' : 'getEmails❌',
        r.smtpReceived ? 'SMTP收信✅' : r.smtpSent ? 'SMTP未收到⚠️' : 'SMTP发送失败❌',
      ].join(' | ');
      console.log(`     ${r.channel.padEnd(22)} ${r.email.padEnd(40)} [${flags}] ${(r.timeMs / 1000).toFixed(1)}s`);
    }
  }

  // 失败的渠道
  if (genFail.length > 0) {
    console.log(`\n  ❌ 不可用渠道 (${genFail.length}):`);
    for (const r of genFail) {
      const errShort = r.generateErr.length > 80 ? r.generateErr.substring(0, 80) + '...' : r.generateErr;
      console.log(`     ${r.channel.padEnd(22)} 错误: ${errShort}`);
    }
  }

  console.log('\n╚══════════════════════════════════════════════════════════════════════════╝');
}

main().catch(console.error);
