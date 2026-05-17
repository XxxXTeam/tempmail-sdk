import { TempEmailClient, setConfig } from '../src';

setConfig({ telemetryEnabled: false });

(async () => {
  const c = new TempEmailClient();
  const i = await c.generate({ channel: 'harakirimail', channelFallback: false });
  if (!i) { console.log('❌ gen=null'); return; }
  console.log('✅ email:', i.email);
  const r = await c.getEmails();
  console.log(`${r.success ? '✅' : '❌'} getEmails: success=${r.success} count=${r.emails.length}`);
})().catch(e => console.error('❌', e.message));
