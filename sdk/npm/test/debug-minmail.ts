import * as minmail from '../src/providers/minmail';

(async () => {
  console.log('Testing minmail generate...');
  const r = await minmail.generateEmail();
  console.log('email:', r.email);
  console.log('token:', r.token);
  if (r.token) {
    try {
      const parsed = JSON.parse(r.token);
      console.log('visitorId:', parsed.visitorId);
      console.log('ck:', parsed.ck);
      console.log('cookie length:', (parsed.cookie || '').length);
    } catch {}
  }

  console.log('\nTesting minmail getEmails...');
  try {
    const emails = await minmail.getEmails(r.email, r.token || '');
    console.log('emails:', emails.length);
  } catch (e: any) {
    console.log('error:', e.message);
  }
})().catch(e => console.error(e.message));
