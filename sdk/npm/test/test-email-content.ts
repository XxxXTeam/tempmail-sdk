import { TempEmailClient, Channel, setConfig } from "../src";
import { sendSmtp } from "./smtp-env";

// 上次测试 SMTP 收信成功的 12 个渠道
const smtpWorkingChannels: Channel[] = [
  "tempmail",
  "ta-easy",
  "10minute-one",
  "mffac",
  "tempmail-lol",
  "chatgpt-org-uk",
  "awamail",
  "mail-tm",
  "dropmail",
  "guerrillamail",
  "maildrop",
  "vip-215",
];

function sleep(ms: number) {
  return new Promise<void>((r) => setTimeout(r, ms));
}

function createMarker(): string {
  return `content-test-${Date.now()}-${Math.random().toString(36).slice(2, 8)}`;
}

async function sendEmail(to: string, marker: string): Promise<boolean> {
  try {
    return await sendSmtp(
      to,
      `Content Test [${marker}]`,
      `这是正文内容测试。标记: ${marker}\n第二行内容用于验证正文完整性。`,
      `<h1>内容测试</h1><p>这是正文内容测试。标记: <b>${marker}</b></p><p>第二行内容用于验证正文完整性。</p>`,
    );
  } catch (e: any) {
    console.log(`  SMTP 发送失败: ${e.message}`);
    return false;
  }
}

interface ContentResult {
  channel: string;
  email: string;
  hasSubject: boolean;
  hasText: boolean;
  hasHtml: boolean;
  subject: string;
  textLen: number;
  htmlLen: number;
  textPreview: string;
  htmlPreview: string;
  issue: string;
}

async function testChannelContent(
  channel: Channel,
): Promise<ContentResult | null> {
  console.log(`\n--- 测试 ${channel} 邮件内容 ---`);

  const client = new TempEmailClient();
  const info = await client.generate({ channel, channelFallback: false });
  if (!info) {
    console.log(`  生成邮箱失败，跳过`);
    return null;
  }
  console.log(`  邮箱: ${info.email}`);

  const marker = createMarker();
  const sent = await sendEmail(info.email, marker);
  if (!sent) return null;

  console.log(`  等待邮件到达...`);

  // 轮询 10 次，每次 5 秒
  for (let i = 1; i <= 10; i++) {
    await sleep(5000);
    const result = await client.getEmails();
    if (!result.success) continue;

    const found = result.emails.find(
      (e) =>
        e.subject?.includes(marker) ||
        e.text?.includes(marker) ||
        e.html?.includes(marker),
    );
    if (found) {
      const r: ContentResult = {
        channel,
        email: info.email,
        hasSubject: !!found.subject && found.subject.trim().length > 0,
        hasText: !!found.text && found.text.trim().length > 0,
        hasHtml: !!found.html && found.html.trim().length > 0,
        subject: found.subject || "(空)",
        textLen: (found.text || "").length,
        htmlLen: (found.html || "").length,
        textPreview: (found.text || "").substring(0, 100),
        htmlPreview: (found.html || "").substring(0, 100),
        issue: "",
      };

      if (!r.hasSubject) r.issue += "缺少Subject; ";
      if (!r.hasText) r.issue += "缺少Text; ";
      if (!r.hasHtml) r.issue += "缺少HTML; ";
      if (!r.issue) r.issue = "正常";

      console.log(`  ✅ 收到邮件`);
      console.log(`    Subject: ${r.subject}`);
      console.log(`    Text长度: ${r.textLen}, HTML长度: ${r.htmlLen}`);
      console.log(`    状态: ${r.issue}`);
      return r;
    }
  }
  console.log(`  ⚠️ 轮询超时未收到`);
  return null;
}

async function main() {
  setConfig({ telemetryEnabled: false });

  console.log("======================================");
  console.log("  邮件内容完整性测试");
  console.log(`  时间: ${new Date().toISOString()}`);
  console.log("======================================");

  const results: ContentResult[] = [];
  for (const ch of smtpWorkingChannels) {
    const r = await testChannelContent(ch);
    if (r) results.push(r);
  }

  console.log("\n\n========== 邮件内容测试汇总 ==========");
  console.log(
    `渠道`.padEnd(22) +
      `Subject`.padEnd(10) +
      `Text`.padEnd(10) +
      `HTML`.padEnd(10) +
      `状态`,
  );
  console.log("-".repeat(70));
  for (const r of results) {
    console.log(
      `${r.channel.padEnd(22)}${(r.hasSubject ? "✅" : "❌").padEnd(10)}${(r.hasText ? `✅(${r.textLen})` : "❌").padEnd(10)}${(r.hasHtml ? `✅(${r.htmlLen})` : "❌").padEnd(10)}${r.issue}`,
    );
  }

  const issues = results.filter((r) => r.issue !== "正常");
  if (issues.length > 0) {
    console.log(`\n⚠️ 有 ${issues.length} 个渠道存在内容问题:`);
    for (const r of issues) {
      console.log(`  ${r.channel}: ${r.issue}`);
      if (r.textPreview) console.log(`    Text预览: ${r.textPreview}`);
      if (r.htmlPreview) console.log(`    HTML预览: ${r.htmlPreview}`);
    }
  } else {
    console.log("\n✅ 所有渠道邮件内容完整");
  }
}

main().catch(console.error);
