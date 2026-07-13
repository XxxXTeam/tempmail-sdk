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
      `NewCh [${marker}]`,
      `Body ${marker}`,
      `<p>Body <b>${marker}</b></p>`,
    );
  } catch (e: any) {
    console.log(`  SMTP失败: ${e.message.substring(0, 60)}`);
    return false;
  }
}

async function test(ch: Channel) {
  console.log(`\n=== ${ch} ===`);
  const c = new TempEmailClient();
  const i = await c.generate({ channel: ch, channelFallback: false });
  if (!i) {
    console.log("  ❌ gen=null");
    return;
  }
  console.log(`  邮箱: ${i.email}`);
  const marker = `new-${ch}-${Date.now()}`;
  if (!(await sendEmail(i.email, marker))) return;
  console.log("  SMTP已发送, 轮询...");
  for (let j = 0; j < 8; j++) {
    await sleep(5000);
    try {
      const r = await c.getEmails();
      if (
        r.success &&
        r.emails.some(
          (e) =>
            e.subject?.includes(marker) ||
            e.text?.includes(marker) ||
            e.html?.includes(marker),
        )
      ) {
        const found = r.emails.find(
          (e) =>
            e.subject?.includes(marker) ||
            e.text?.includes(marker) ||
            e.html?.includes(marker),
        )!;
        console.log(
          `  ✅ 收到! text(${(found.text || "").length}) html(${(found.html || "").length})`,
        );
        return;
      }
    } catch {}
  }
  console.log("  ⚠️ 超时");
}

async function main() {
  const chs: Channel[] = [
    "catchmail-mailistry",
    "catchmail-zeppost",
    "mailforspam-tempmail-io",
    "mailforspam-disposable",
    "guerrillamail-com",
    "sharklasers-com",
    "grr-la-com",
  ];
  for (const ch of chs) await test(ch);
}
main().catch(console.error);
