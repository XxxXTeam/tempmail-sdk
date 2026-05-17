import { TempEmailClient, setConfig } from '../src';
setConfig({ telemetryEnabled: false });

const channels = ['mail-gw', 'tempmail-lol-v2', 'sharklasers', 'grr-la', 'guerrillamail-info'] as const;

async function main() {
  for (const ch of channels) {
    console.log(`\n--- ${ch} ---`);
    try {
      const c = new TempEmailClient();
      const i = await c.generate({ channel: ch, channelFallback: false });
      if (!i) { console.log('  ❌ gen=null'); continue; }
      console.log(`  ✅ ${i.email}`);
      const r = await c.getEmails();
      console.log(`  ${r.success ? '✅' : '❌'} getEmails=${r.success} (${r.emails.length})`);
    } catch (e: any) { console.log(`  ❌ ${e.message?.substring(0,60)}`); }
  }
}
main().catch(console.error);
