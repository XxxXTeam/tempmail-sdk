import { TempEmailClient, Channel, setConfig } from "../src";
import { sendSmtp } from "./smtp-env";

setConfig({ telemetryEnabled: false });

function sleep(ms: number) {
  return new Promise<void>((r) => setTimeout(r, ms));
}

async function sendEmail(to: string, marker: string): Promise<boolean> {
  try {
    return await sendSmtp(
      to,
      `Fix2 [${marker}]`,
      `正文 ${marker}`,
      `<p>正文 <b>${marker}</b></p>`,
    );
  } catch (e: any) {
    console.log(`  SMTP失败: ${e.message}`);
    return false;
  }
}

async function test(channel: Channel) {
  console.log(`\n=== ${channel} ===`);
  const client = new TempEmailClient();
  try {
    const info = await client.generate({ channel, channelFallback: false });
    if (!info) {
      console.log("  ❌ 生成失败");
      return;
    }
    console.log(`  邮箱: ${info.email}`);

    const marker = `fix2-${channel}-${Date.now()}`;
    if (!(await sendEmail(info.email, marker))) return;
    console.log("  SMTP发送, 等待...");

    for (let i = 1; i <= 8; i++) {
      await sleep(5000);
      try {
        const r = await client.getEmails();
        if (!r.success) {
          console.log(`  轮询${i}: fail`);
          continue;
        }
        const found = r.emails.find(
          (e) =>
            e.subject?.includes(marker) ||
            e.text?.includes(marker) ||
            e.html?.includes(marker),
        );
        if (found) {
          console.log(`  ✅ 收到! Subject=${found.subject}`);
          console.log(
            `    Text(${(found.text || "").length}): ${(found.text || "").substring(0, 60)}`,
          );
          console.log(
            `    HTML(${(found.html || "").length}): ${(found.html || "").substring(0, 60)}`,
          );
          return;
        }
        console.log(`  轮询${i}: ${r.emails.length}封`);
      } catch (e: any) {
        console.log(`  轮询${i}: 异常 ${e.message.substring(0, 60)}`);
      }
    }
    console.log("  ⚠️ 超时");
  } catch (e: any) {
    console.log(`  ❌ ${e.message}`);
  }
}

async function main() {
  await test("tempmail-cn");
  await test("10minute-one");
  await test("catchmail-mailistry");
  await test("mailforspam-disposable");
  await test("guerrillamail-com");
}
main().catch(console.error);
