import { TempEmailClient, Channel, setConfig, listChannels } from '../src';
import { sendSmtp } from './smtp-env';

setConfig({ telemetryEnabled: false });

const ALL: Channel[] = listChannels().map((info) => info.channel);

function sleep(ms: number) { return new Promise<void>(r => setTimeout(r, ms)); }

async function send(to: string, marker: string): Promise<boolean> {
  try { return await sendSmtp(to, `Body [${marker}]`, `纯文本正文 marker=${marker}`, `<h1>HTML正文</h1><p>marker=<b>${marker}</b></p>`); }
  catch { return false; }
}

interface R { ch: string; gen: boolean; email: string; smtp: boolean; recv: boolean;
  subj: string; textLen: number; htmlLen: number; issue: string; time: number; }

async function test(ch: Channel): Promise<R> {
  const r: R = { ch, gen:false, email:'', smtp:false, recv:false, subj:'', textLen:0, htmlLen:0, issue:'', time:0 };
  const t0 = Date.now();
  try {
    const c = new TempEmailClient();
    const i = await c.generate({ channel: ch, channelFallback: false });
    if (!i) { r.issue='gen=null'; r.time=Date.now()-t0; return r; }
    r.gen=true; r.email=i.email;
    const mk = `body-${ch}-${Date.now()}`;
    r.smtp = await send(i.email, mk);
    if (!r.smtp) { r.issue='smtp_fail'; r.time=Date.now()-t0; return r; }
    for (let j=0; j<10; j++) {
      await sleep(5000);
      try {
        const res = await c.getEmails();
        if (!res.success) continue;
        const f = res.emails.find(e => e.subject?.includes(mk)||e.text?.includes(mk)||e.html?.includes(mk));
        if (f) {
          r.recv=true; r.subj=f.subject||''; r.textLen=(f.text||'').length; r.htmlLen=(f.html||'').length;
          if (!f.subject) r.issue+='无Subject;';
          if (!f.text || f.text.trim().length===0) r.issue+='无Text;';
          if (!f.html || f.html.trim().length===0) r.issue+='无HTML;';
          if (!r.issue) r.issue='OK';
          break;
        }
      } catch {}
    }
    if (!r.recv) r.issue='未收到SMTP';
  } catch (e:any) { r.issue=`err:${e.message?.substring(0,40)}`; }
  r.time=Date.now()-t0;
  return r;
}

async function main() {
  console.log(`全渠道正文内容测试 (${ALL.length}渠道) ${new Date().toISOString()}\n`);
  const results: R[] = [];
  for (const ch of ALL) {
    process.stdout.write(`[${ch}] `);
    const r = await test(ch);
    console.log(r.gen ? (r.recv ? `✅ ${r.issue} text=${r.textLen} html=${r.htmlLen}` : `⚠️ ${r.issue}`) : `❌ ${r.issue}`);
    results.push(r);
  }
  console.log('\n\n========== 正文内容测试汇总 ==========');
  console.log('渠道'.padEnd(20)+'生成'.padEnd(5)+'收信'.padEnd(5)+'Subject'.padEnd(9)+'Text'.padEnd(12)+'HTML'.padEnd(12)+'状态');
  console.log('-'.repeat(85));
  for (const r of results) {
    const g = r.gen?'✅':'❌';
    const s = r.recv?'✅':'—';
    const su = r.recv?(r.subj?'✅':'❌'):'—';
    const tx = r.recv?(r.textLen>0?`✅(${r.textLen})`:'❌(0)'):'—';
    const ht = r.recv?(r.htmlLen>0?`✅(${r.htmlLen})`:'❌(0)'):'—';
    console.log(`${r.ch.padEnd(20)}${g.padEnd(5)}${s.padEnd(5)}${su.padEnd(9)}${tx.padEnd(12)}${ht.padEnd(12)}${r.issue}`);
  }
  const hasContent = results.filter(r=>r.recv);
  const noText = hasContent.filter(r=>r.textLen===0);
  const noHtml = hasContent.filter(r=>r.htmlLen===0);
  if (noText.length||noHtml.length) {
    console.log(`\n⚠️ 正文问题:`);
    for (const r of noText) console.log(`  ${r.ch}: 缺少Text`);
    for (const r of noHtml) console.log(`  ${r.ch}: 缺少HTML`);
  }
}
main().catch(console.error);
