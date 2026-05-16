/** 经 Cloudflare MX 向 smail.pw 地址投递探针邮件 */
import nodemailer from 'nodemailer';

const to = process.argv[2];
const marker = process.argv[3] || `probe-${Date.now()}`;
if (!to) {
  console.error('用法: npx ts-node scripts/smail-send-probe.ts <to@smail.pw> [marker]');
  process.exit(1);
}

async function main(): Promise<void> {
  const transport = nodemailer.createTransport({
    host: 'route1.mx.cloudflare.net',
    port: 25,
    secure: false,
    tls: { rejectUnauthorized: false },
    connectionTimeout: 30000,
  });
  const info = await transport.sendMail({
    from: 'probe@tempmail-sdk.test',
    to,
    subject: `smail-sdk ${marker}`,
    text: `probe body ${marker}`,
    html: `<p>probe html <b>${marker}</b></p>`,
  });
  console.log('sent:', info.messageId || info.response);
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
