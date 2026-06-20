import { fetchWithTimeout } from '../src/retry';
import { sendSmtp } from './smtp-env';

(async () => {
  const r1 = await fetchWithTimeout('https://email10min.com/zh', {headers:{'User-Agent':'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36'}});
  const h = r1.headers as any;
  const cookies = typeof h.getSetCookie === 'function' ? h.getSetCookie().map((l:string)=>l.split(';')[0]).join('; ') : '';
  const html = await r1.text();
  const csrf = (html.match(/<meta\s+name="csrf-token"\s+content="([^"]+)"/i)||[])[1]||'';

  const r2 = await fetchWithTimeout('https://email10min.com/messages?'+Date.now(), {
    method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded','Cookie':cookies,
      'X-Requested-With':'XMLHttpRequest','User-Agent':'Mozilla/5.0'}, body:'_token='+encodeURIComponent(csrf)+'&captcha=' });
  const d1 = await r2.json() as any;
  const email = d1.mailbox;
  console.log('email:', email);

  await sendSmtp(email, 'RawTest', 'TextBody123', '<p>HtmlBody123</p>');
  console.log('sent, waiting 15s...');
  await new Promise(r=>setTimeout(r,15000));

  const r3 = await fetchWithTimeout('https://email10min.com/messages?'+Date.now(), {
    method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded','Cookie':cookies,
      'X-Requested-With':'XMLHttpRequest','User-Agent':'Mozilla/5.0'}, body:'_token='+encodeURIComponent(csrf)+'&captcha=' });
  const raw = await r3.text();
  console.log('RAW RESPONSE (first 800):');
  console.log(raw.substring(0, 800));
  const data = JSON.parse(raw);
  if (data.messages?.length) {
    console.log('\nFirst msg ALL keys:', Object.keys(data.messages[0]));
    console.log('First msg JSON:', JSON.stringify(data.messages[0]).substring(0, 500));
  }
})().catch(e=>console.error(e.message));
