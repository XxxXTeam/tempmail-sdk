import { TempEmailClient, setConfig } from '../src';
import { sendSmtp } from './smtp-env';

setConfig({ telemetryEnabled: false });
const sleep = (ms: number) => new Promise<void>(r => setTimeout(r, ms));

(async () => {
  console.log('=== tempmail-plus SMTP 正文测试 ===');
  const c = new TempEmailClient();
  const i = await c.generate({ channel: 'tempmail-plus', channelFallback: false });
  if (!i) { console.log('❌ gen=null'); return; }
  console.log(`✅ email: ${i.email}`);

  const mk = `plus-${Date.now()}`;
  await sendSmtp(i.email, `Plus Test [${mk}]`, `纯文本正文 ${mk}`, `<h1>HTML正文</h1><p>marker: <b>${mk}</b></p>`);
  console.log('SMTP 已发送, 等待收信...');

  for (let j = 0; j < 10; j++) {
    await sleep(5000);
    const r = await c.getEmails();
    if (!r.success) { console.log(`  poll ${j+1}: fail`); continue; }
    const f = r.emails.find(e => e.subject?.includes(mk) || e.text?.includes(mk) || e.html?.includes(mk));
    if (f) {
      console.log(`✅ 收到邮件!`);
      console.log(`  Subject: ${f.subject}`);
      console.log(`  Text长度: ${(f.text||'').length}  预览: ${(f.text||'').substring(0,60)}`);
      console.log(`  HTML长度: ${(f.html||'').length}  预览: ${(f.html||'').substring(0,60)}`);
      console.log(`  From: ${f.from}`);
      return;
    }
    console.log(`  poll ${j+1}: ${r.emails.length}封, 未匹配`);
  }
  console.log('⚠️ 超时');
})().catch(e => console.error('❌', e.message));
