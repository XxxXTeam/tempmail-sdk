import { TempEmailClient, setConfig } from "../src";

setConfig({ telemetryEnabled: false });

const channels = [
  "catchmail-mailistry",
  "catchmail-zeppost",
  "mailforspam-tempmail-io",
  "mailforspam-disposable",
  "guerrillamail-com",
  "sharklasers-com",
  "grr-la-com",
] as const;

async function main() {
  for (const ch of channels) {
    console.log(`\n--- ${ch} ---`);
    try {
      const c = new TempEmailClient();
      const i = await c.generate({ channel: ch, channelFallback: false });
      if (!i) {
        console.log("  ❌ gen=null");
        continue;
      }
      console.log(`  ✅ email: ${i.email}`);
      const r = await c.getEmails();
      console.log(
        `  ${r.success ? "✅" : "❌"} getEmails: success=${r.success} count=${r.emails.length}`,
      );
    } catch (e: any) {
      console.log(`  ❌ error: ${e.message?.substring(0, 80)}`);
    }
  }
}

main().catch(console.error);
