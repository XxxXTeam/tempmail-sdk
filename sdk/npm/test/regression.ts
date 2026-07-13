import { TempEmailClient, Channel, setConfig, listChannels } from "../src";
import { sendSmtp } from "./smtp-env";

setConfig({ telemetryEnabled: false });

const allChannels: Channel[] = listChannels().map((info) => info.channel);

function sleep(ms: number) {
  return new Promise<void>((r) => setTimeout(r, ms));
}

async function sendEmail(to: string, marker: string): Promise<boolean> {
  try {
    return await sendSmtp(
      to,
      `Regression [${marker}]`,
      `Body ${marker}`,
      `<p>Body <b>${marker}</b></p>`,
    );
  } catch {
    return false;
  }
}

interface Result {
  channel: string;
  genOk: boolean;
  email: string;
  getOk: boolean;
  smtpSent: boolean;
  smtpRecv: boolean;
  genErr: string;
  getErr: string;
  timeMs: number;
}

async function test(ch: Channel): Promise<Result> {
  const r: Result = {
    channel: ch,
    genOk: false,
    email: "",
    getOk: false,
    smtpSent: false,
    smtpRecv: false,
    genErr: "",
    getErr: "",
    timeMs: 0,
  };
  const t0 = Date.now();
  console.log(`\n[${ch}]`);
  try {
    const client = new TempEmailClient();
    const info = await client.generate({ channel: ch, channelFallback: false });
    if (!info) {
      r.genErr = "null";
      console.log("  ❌ gen=null");
      r.timeMs = Date.now() - t0;
      return r;
    }
    r.genOk = true;
    r.email = info.email;
    console.log(`  ✅ ${info.email}`);
    try {
      const res = await client.getEmails();
      r.getOk = res.success;
      console.log(
        `  ${res.success ? "✅" : "⚠️"} getEmails=${res.success} (${res.emails.length})`,
      );
    } catch (e: any) {
      r.getErr = e.message;
      console.log(`  ❌ getEmails: ${e.message.substring(0, 50)}`);
    }
    const marker = `reg-${ch}-${Date.now()}`;
    r.smtpSent = await sendEmail(info.email, marker);
    if (r.smtpSent) {
      for (let i = 0; i < 8; i++) {
        await sleep(5000);
        try {
          const res = await client.getEmails();
          if (
            res.success &&
            res.emails.some(
              (e) =>
                e.subject?.includes(marker) ||
                e.text?.includes(marker) ||
                e.html?.includes(marker),
            )
          ) {
            r.smtpRecv = true;
            break;
          }
        } catch {}
      }
      console.log(`  ${r.smtpRecv ? "✅" : "⚠️"} SMTP recv=${r.smtpRecv}`);
    }
  } catch (e: any) {
    r.genErr = e.message;
    console.log(`  ❌ ${e.message.substring(0, 60)}`);
  }
  r.timeMs = Date.now() - t0;
  return r;
}

async function main() {
  console.log(`=== 回归测试 (${allChannels.length} 渠道) ===`);
  console.log(`时间: ${new Date().toISOString()}\n`);
  const results: Result[] = [];
  for (const ch of allChannels) results.push(await test(ch));

  console.log("\n\n========== 回归测试汇总 ==========");
  const ok = results.filter((r) => r.genOk);
  const fail = results.filter((r) => !r.genOk);
  const getOk = results.filter((r) => r.getOk);
  const recv = results.filter((r) => r.smtpRecv);
  console.log(`生成成功: ${ok.length}/${results.length}`);
  console.log(`获取成功: ${getOk.length}/${results.length}`);
  console.log(`SMTP收信: ${recv.length}/${ok.length}`);
  console.log(
    "\n渠道".padEnd(22) +
      "生成".padEnd(6) +
      "获取".padEnd(6) +
      "SMTP".padEnd(6) +
      "耗时",
  );
  console.log("-".repeat(60));
  for (const r of results) {
    console.log(
      `${r.channel.padEnd(22)}${(r.genOk ? "✅" : "❌").padEnd(6)}${(r.getOk ? "✅" : "❌").padEnd(6)}${(r.smtpRecv ? "✅" : r.smtpSent ? "⚠️" : "—").padEnd(6)}${(r.timeMs / 1000).toFixed(1)}s`,
    );
  }
  if (fail.length) {
    console.log(`\n❌ 失败渠道:`);
    for (const r of fail)
      console.log(`  ${r.channel}: ${r.genErr.substring(0, 80)}`);
  }
}
main().catch(console.error);
