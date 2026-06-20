import { TempEmailClient, setConfig, setLogLevel, LogLevel } from '../src';

setConfig({ telemetryEnabled: false });
setLogLevel(LogLevel.DEBUG);

async function main() {
  console.log('=== 10minute-one getEmails debug ===');
  const c1 = new TempEmailClient();
  const i1 = await c1.generate({ channel: '10minute-one', channelFallback: false });
  if (!i1) { console.log('GENERATE FAILED'); return; }
  console.log(`Email: ${i1.email}`);

  try {
    const r = await c1.getEmails();
    console.log(`getEmails result: success=${r.success} count=${r.emails.length}`);
  } catch (e: any) {
    console.log(`getEmails THREW: ${e.message}`);
    console.log(e.stack);
  }

  console.log('\n=== catchmail-mailistry getEmails debug ===');
  const c2 = new TempEmailClient();
  const i2 = await c2.generate({ channel: 'catchmail-mailistry', channelFallback: false });
  if (!i2) { console.log('GENERATE FAILED'); return; }
  console.log(`Email: ${i2.email}`);

  try {
    const r2 = await c2.getEmails();
    console.log(`getEmails result: success=${r2.success} count=${r2.emails.length}`);
  } catch (e: any) {
    console.log(`getEmails THREW: ${e.message}`);
    console.log(e.stack);
  }
}

main().catch(console.error);
