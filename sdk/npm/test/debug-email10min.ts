import * as e10 from '../src/providers/email10min';
import nodemailer from 'nodemailer';

(async () => {
  const r = await e10.generateEmail();
  console.log('email:', r.email, 'token:', !!r.token);
  const t = nodemailer.createTransport({host:'smtp.exmail.qq.com',port:465,secure:true,
    auth:{user:'supper@openel.top',pass:'PKZT5rgvUvGdgcxe'},connectionTimeout:15000});
  const mk = 'dbg-'+Date.now();
  await t.sendMail({from:'supper@openel.top',to:r.email,
    subject:`Debug [${mk}]`,text:`Text body ${mk}`,html:`<p>HTML <b>${mk}</b></p>`});
  console.log('sent, waiting...');
  for (let i=0;i<6;i++) {
    await new Promise(r=>setTimeout(r,5000));
    const emails = await e10.getEmails(r.email, r.token!);
    console.log(`poll ${i+1}: ${emails.length} emails`);
    for (const e of emails) {
      console.log(JSON.stringify({
        id:e.id, subject:e.subject, from:e.from,
        textLen:e.text?.length, htmlLen:e.html?.length,
        textPre:e.text?.substring(0,50), htmlPre:e.html?.substring(0,50),
      }));
    }
    if (emails.length > 0) break;
  }
})().catch(e=>console.error(e.message));
