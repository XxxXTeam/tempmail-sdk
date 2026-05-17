import { TempEmailClient, setConfig, setLogLevel, LogLevel } from '../src';
setConfig({ telemetryEnabled: false });
setLogLevel(LogLevel.DEBUG);

async function main() {
  for (const ch of ['tempmail-lol-v2', 'sharklasers'] as const) {
    console.log(`\n=== ${ch} ===`);
    const c = new TempEmailClient();
    const i = await c.generate({ channel: ch, channelFallback: false });
    console.log('result:', i?.email || 'null');
  }
}
main().catch(console.error);
