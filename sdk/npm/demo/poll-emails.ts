/**
 * Demo: 获取指定渠道邮箱，轮询获取邮件
 *
 * 使用方法:
 *   npx ts-node demo/poll-emails.ts
 *
 * 行为:
 * - 已设置 SMTP_HOST：自动对全部渠道（可用 POLL_CHANNELS 限定）创建临时邮箱、经 SMTP 发送探针邮件并轮询是否收到，用于验收渠道是否可用。
 * - 未设置 SMTP_HOST：交互式选择单个渠道并轮询（原行为）。
 * - 已设置 SMTP 时仍想交互：加参数 --interactive 或 -i
 *
 * SMTP 环境变量（自动模式必填 SMTP_HOST）:
 *   SMTP_HOST      服务器地址
 *   SMTP_PORT      端口，默认 587
 *   SMTP_SECURE    含义随端口变化：
 *                  - 465 / smtps / implicit：隐式 TLS（整条连接即 SSL，对应 nodemailer secure:true）
 *                  - 587、2525、25：不使用 secure:true；1/true 表示「强制 STARTTLS」（requireTLS）
 *                  - 其它端口：1/true 视为隐式 TLS（secure:true）
 *   SMTP_REQUIRE_TLS  是否强制 STARTTLS（仅非隐式 TLS 时有效）。未设时 587/2525 默认强制，25 默认不强制
 *   SMTP_IGNORE_TLS   1/true 则禁用 STARTTLS（明文，仅调试）
 *   SMTP_TLS_REJECT_UNAUTHORIZED  默认校验证书；0/false 允许自签名（仅调试）
 *   SMTP_USER      用户名（可选，无认证可留空）
 *   SMTP_PASS      密码（可选）
 *   SMTP_FROM      发件人地址，默认 SMTP_USER，再否则 noreply@localhost
 *
 * 可选:
 *   POLL_CHANNELS       逗号分隔的渠道 id，仅测这些；未设则测全部
 *   POLL_INTERVAL_MS    轮询间隔毫秒，默认 5000
 *   POLL_MAX_ROUNDS       每渠道最大轮询次数，默认 60
 */

import { randomBytes } from 'crypto';
import * as readline from 'readline';
import nodemailer from 'nodemailer';
import {
  generateEmail,
  getEmails,
  listChannels,
  getChannelInfo,
  Channel,
  EmailInfo,
  ChannelInfo,
  Email,
} from '../src';

const POLL_INTERVAL = parseInt(process.env.POLL_INTERVAL_MS || '5000', 10);
const MAX_POLL_COUNT = parseInt(process.env.POLL_MAX_ROUNDS || '60', 10);

interface SmtpConfig {
  host: string;
  port: number;
  /** true = 隐式 TLS（SMTPS，典型端口 465） */
  secure: boolean;
  /** 明文连接后强制升级到 TLS（587 提交端口） */
  requireTLS: boolean;
  ignoreTLS: boolean;
  tls?: { rejectUnauthorized?: boolean };
  user?: string;
  pass?: string;
  from: string;
}

function printJson(label: string, data: unknown): void {
  console.log(`\n${label}:`);
  console.log(JSON.stringify(data, null, 2));
}

function sleep(ms: number): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, ms));
}

function question(prompt: string): Promise<string> {
  const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout,
  });
  return new Promise((resolve) => {
    rl.question(prompt, (answer: string) => {
      rl.close();
      resolve(answer.trim());
    });
  });
}

function envBool(v: string | undefined, defaultVal: boolean): boolean {
  if (v === undefined || v.trim() === '') {
    return defaultVal;
  }
  const s = v.trim().toLowerCase();
  if (['1', 'true', 'yes'].includes(s)) {
    return true;
  }
  if (['0', 'false', 'no'].includes(s)) {
    return false;
  }
  return defaultVal;
}

function loadSmtpConfig(): SmtpConfig | null {
  const host = process.env.SMTP_HOST?.trim();
  if (!host) {
    return null;
  }
  const port = parseInt(process.env.SMTP_PORT || '587', 10);
  const sec = process.env.SMTP_SECURE?.trim().toLowerCase();

  const submissionPorts = new Set([25, 587, 2525]);
  const userWantsEncryption =
    sec === '1' || sec === 'true' || sec === 'yes' || sec === 'implicit' || sec === 'smtps';

  /** 465 或显式 smtps / implicit → 连接即 TLS */
  const implicitTls =
    port === 465 || sec === 'implicit' || sec === 'smtps';

  let secure = false;
  if (implicitTls) {
    secure = true;
  } else if (!submissionPorts.has(port) && userWantsEncryption) {
    secure = true;
  }
  /*
   * 587/2525/25 必须用 secure:false，由 STARTTLS 升级；此前把 SMTP_SECURE=1 当成 secure:true
   * 会导致与提交端口不兼容（对方期待明文握手再 STARTTLS）。
   */
  const ignoreTLS = envBool(process.env.SMTP_IGNORE_TLS, false);

  const defaultRequireStartTls =
    !secure &&
    !ignoreTLS &&
    (port === 587 || port === 2525 || (port === 25 && userWantsEncryption));
  const requireTLS = envBool(process.env.SMTP_REQUIRE_TLS, defaultRequireStartTls);

  const rejectUnauthorized = envBool(process.env.SMTP_TLS_REJECT_UNAUTHORIZED, true);

  const user = process.env.SMTP_USER?.trim() || undefined;
  const pass = process.env.SMTP_PASS?.trim() || undefined;
  const from =
    process.env.SMTP_FROM?.trim() ||
    user ||
    'noreply@localhost';
  return {
    host,
    port,
    secure,
    requireTLS,
    ignoreTLS,
    tls: { rejectUnauthorized },
    user,
    pass,
    from,
  };
}

function channelsToTest(all: ChannelInfo[]): Channel[] {
  const raw = process.env.POLL_CHANNELS?.trim();
  if (!raw) {
    return all.map((c) => c.channel);
  }
  const wanted = raw.split(',').map((s) => s.trim()).filter(Boolean);
  const valid = new Set<Channel>(all.map((c) => c.channel));
  const picked: Channel[] = [];
  for (const id of wanted) {
    if (valid.has(id as Channel)) {
      picked.push(id as Channel);
    } else {
      console.warn(`未知渠道，已跳过: ${id}`);
    }
  }
  return picked.length > 0 ? picked : all.map((c) => c.channel);
}

async function sendProbeMail(config: SmtpConfig, to: string, subject: string, text: string): Promise<void> {
  const transporter = nodemailer.createTransport({
    host: config.host,
    port: config.port,
    secure: config.secure,
    requireTLS: config.requireTLS,
    ignoreTLS: config.ignoreTLS,
    tls: config.tls,
    auth:
      config.user !== undefined && config.pass !== undefined
        ? { user: config.user, pass: config.pass }
        : undefined,
  });
  await transporter.sendMail({
    from: config.from,
    to,
    subject,
    text,
  });
}

async function selectChannel(channels: ChannelInfo[]): Promise<Channel> {
  console.log('\n请选择渠道:');
  channels.forEach((ch, index) => {
    console.log(`  [${index + 1}] ${ch.channel} - ${ch.name} (${ch.website})`);
  });

  while (true) {
    const input = await question('\n请输入渠道编号 (1-' + channels.length + '): ');
    const num = parseInt(input, 10);

    if (num >= 1 && num <= channels.length) {
      return channels[num - 1].channel;
    }
    console.log('❌ 无效的编号，请重新输入');
  }
}

function printEmail(email: Email, index: number): void {
  console.log(`\n邮件 #${index + 1}`);
  console.log('━'.repeat(50));
  console.log(`ID: ${email.id || '无'}`);
  console.log(`发件人: ${email.from || '未知'}`);
  console.log(`收件人: ${email.to || '未知'}`);
  console.log(`主题: ${email.subject || '无主题'}`);
  console.log(`时间: ${email.date || '未知'}`);
  console.log(`已读: ${email.isRead ? '是' : '否'}`);
  if (email.text) {
    const preview = email.text.substring(0, 200);
    console.log(`内容: ${preview}`);
  }
  if (email.html) {
    const htmlPreview = email.html.replace(/<[^>]*>/g, '').substring(0, 100);
    console.log(`HTML: ${htmlPreview}...`);
  }
  if (email.attachments.length > 0) {
    console.log(`附件: ${email.attachments.map(a => a.filename).join(', ')}`);
  }
  console.log('━'.repeat(50));
}

async function pollEmails(emailInfo: EmailInfo): Promise<void> {
  console.log(`\n开始轮询邮件...（每 ${POLL_INTERVAL / 1000} 秒检查一次）`);
  console.log('按 Ctrl+C 停止轮询\n');

  let pollCount = 0;

  while (pollCount < MAX_POLL_COUNT) {
    pollCount++;

    try {
      const result = await getEmails(emailInfo);

      const timestamp = new Date().toLocaleTimeString();

      if (result.emails.length > 0) {
        console.log(`\n[${timestamp}] 🎉 收到 ${result.emails.length} 封邮件！\n`);

        printJson('返回数据 (GetEmailsResult)', result);

        for (let i = 0; i < result.emails.length; i++) {
          printEmail(result.emails[i], i);
        }
      } else {
        process.stdout.write(`\r[${timestamp}] 第 ${pollCount}/${MAX_POLL_COUNT} 次检查，暂无新邮件...`);
      }
    } catch (error) {
      console.error(`\n轮询出错: ${error}`);
    }

    await sleep(POLL_INTERVAL);
  }

  console.log('\n\n已达到最大轮询次数，停止轮询');
}

type ProbeResult = 'ok' | 'fail_create' | 'fail_smtp' | 'fail_timeout';

async function probeChannel(channel: Channel, smtp: SmtpConfig): Promise<ProbeResult> {
  const emailInfo = await generateEmail({ channel, channelFallback: false });
  if (!emailInfo) {
    console.log('  ❌ 创建临时邮箱失败');
    return 'fail_create';
  }

  const to = String(emailInfo.email ?? '').trim();
  if (!to || !to.includes('@')) {
    console.log('  ❌ 无效收件地址（渠道未返回邮箱，例如 linshi-email 触发频率限制时 API 会返回空 data）');
    return 'fail_create';
  }

  const marker = `tm-sdk-${Date.now()}-${randomBytes(4).toString('hex')}`;
  const subject = `[tempmail-sdk] ${marker}`;
  const text = `自动化探针邮件\nmarker: ${marker}\nchannel: ${channel}\n`;

  try {
    await sendProbeMail(smtp, to, subject, text);
  } catch (err) {
    console.log(`  ❌ SMTP 发送失败: ${err}`);
    return 'fail_smtp';
  }

  console.log(`  📤 已发送测试邮件 → ${to}`);

  let probeMismatchLogged = false;
  for (let i = 0; i < MAX_POLL_COUNT; i++) {
    const result = await getEmails(emailInfo);
    if (result.success && result.emails.length > 0) {
      const hit = result.emails.some(
        (e) =>
          (e.subject && e.subject.includes(marker)) ||
          (e.text && e.text.includes(marker)) ||
          (e.html && e.html.includes(marker)),
      );
      if (hit) {
        console.log(`  ✅ 已收到匹配邮件（第 ${i + 1} 次轮询）`);
        return 'ok';
      }
      const hintChannel = channel === 'smail-pw' || channel === 'chatgpt-org-uk';
      if (hintChannel && !probeMismatchLogged) {
        probeMismatchLogged = true;
        const subs = result.emails.map((e) => e.subject || '(无主题)').join('; ');
        console.log(`\n  提示: 已解析到 ${result.emails.length} 封邮件，但主题/正文/HTML 未含探针标记。主题摘要: ${subs.slice(0, 200)}`);
      }
    }
    process.stdout.write(`\r  轮询 ${i + 1}/${MAX_POLL_COUNT}...`);
    await sleep(POLL_INTERVAL);
  }

  console.log('\n  ❌ 超时未收到带标记的邮件');
  if (channel === 'smail-pw') {
    console.log(
      '  提示: smail.pw 依赖 RSC 响应解析；若已修复解析仍超时，可能是外发 SMTP 被拒/延迟，可加大 POLL_MAX_ROUNDS、POLL_INTERVAL_MS 后重试。',
    );
  }
  return 'fail_timeout';
}

async function runSmtpAutoSuite(smtp: SmtpConfig): Promise<void> {
  console.log('═'.repeat(50));
  console.log('  临时邮箱 Demo — SMTP 自动探针（默认测全部渠道）');
  console.log('═'.repeat(50));
  console.log(
    `\nSMTP: ${smtp.host}:${smtp.port} implicitTLS=${smtp.secure} requireTLS=${smtp.requireTLS} ignoreTLS=${smtp.ignoreTLS} tls.rejectUnauthorized=${smtp.tls?.rejectUnauthorized ?? true} from=${smtp.from}`,
  );

  const all = listChannels();
  const targets = channelsToTest(all);
  console.log(`\n待测渠道 (${targets.length}): ${targets.join(', ')}\n`);

  const stats: Record<ProbeResult, number> = {
    ok: 0,
    fail_create: 0,
    fail_smtp: 0,
    fail_timeout: 0,
  };

  for (const channel of targets) {
    const info = getChannelInfo(channel);
    const label = info ? `${info.name} (${channel})` : channel;
    console.log(`\n── ${label} ──`);
    const r = await probeChannel(channel, smtp);
    stats[r]++;
  }

  console.log('\n' + '═'.repeat(50));
  console.log('  汇总');
  console.log('═'.repeat(50));
  console.log(`  成功收到:     ${stats.ok}`);
  console.log(`  创建失败:     ${stats.fail_create}`);
  console.log(`  SMTP 失败:    ${stats.fail_smtp}`);
  console.log(`  超时未收到:   ${stats.fail_timeout}`);

  const bad = stats.fail_create + stats.fail_smtp + stats.fail_timeout;
  if (bad > 0) {
    process.exitCode = 1;
  }
}

async function runInteractive(): Promise<void> {
  console.log('═'.repeat(50));
  console.log('  临时邮箱 Demo - 获取邮箱并轮询邮件');
  console.log('═'.repeat(50));

  console.log('\n[1] 列出所有支持的渠道...');
  const channels = listChannels();
  printJson('支持的渠道列表', channels);

  console.log('\n[2] 选择渠道...');
  const selectedChannel = await selectChannel(channels);

  console.log(`\n[3] 获取渠道 "${selectedChannel}" 信息...`);
  const channelInfo = getChannelInfo(selectedChannel);
  printJson('渠道信息', channelInfo);

  console.log(`\n[4] 从 ${selectedChannel} 渠道获取临时邮箱...`);

  try {
    const emailInfo = await generateEmail({ channel: selectedChannel });

    if (!emailInfo) {
      console.error('\n❌ 所有渠道均不可用');
      process.exit(1);
      return;
    }

    console.log('\n✅ 获取邮箱成功！');
    printJson('返回数据 (EmailInfo)', emailInfo);

    console.log('\n📋 邮箱信息:');
    console.log(`   渠道: ${emailInfo.channel}`);
    console.log(`   邮箱: ${emailInfo.email}`);
    if (emailInfo.expiresAt) {
      console.log(`   过期时间: ${emailInfo.expiresAt}`);
    }
    if (emailInfo.createdAt) {
      console.log(`   创建时间: ${emailInfo.createdAt}`);
    }

    console.log('\n📧 请发送邮件到以上邮箱地址进行测试');

    await pollEmails(emailInfo);
  } catch (error) {
    console.error(`\n❌ 获取邮箱失败: ${error}`);
    process.exit(1);
  }
}

async function main(): Promise<void> {
  const argv = new Set(process.argv.slice(2));
  const forceInteractive = argv.has('--interactive') || argv.has('-i');
  const smtp = loadSmtpConfig();

  if (smtp && !forceInteractive) {
    await runSmtpAutoSuite(smtp);
    return;
  }

  await runInteractive();
}

main().catch(console.error);
