import nodemailer from 'nodemailer';
import { Channel, TempEmailClient, setConfig } from '../src';

setConfig({ telemetryEnabled: false });

const channels = (process.env.CHANNELS || 'catchmail')
  .split(',')
  .map((s) => s.trim())
  .filter(Boolean) as Channel[];

const smtp = {
  host: process.env.SMTP_HOST || '',
  port: Number(process.env.SMTP_PORT || 465),
  user: process.env.SMTP_USER || '',
  pass: process.env.SMTP_PASS || '',
  from: process.env.SMTP_FROM || process.env.SMTP_USER || '',
};

function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function assertSmtpEnv(): void {
  for (const [key, value] of Object.entries(smtp)) {
    if (key === 'port') continue;
    if (!value) throw new Error(`missing SMTP_${key.toUpperCase()}`);
  }
}

async function send(to: string, marker: string): Promise<{ textMarker: string; htmlMarker: string }> {
  const textMarker = `${marker}-TEXT`;
  const htmlMarker = `${marker}-HTML`;
  const transporter = nodemailer.createTransport({
    host: smtp.host,
    port: smtp.port,
    secure: smtp.port === 465,
    auth: { user: smtp.user, pass: smtp.pass },
    connectionTimeout: 15000,
  });
  await transporter.sendMail({
    from: smtp.from,
    to,
    subject: marker,
    text: `plain ${textMarker}`,
    html: `<html><body><p>html <b>${htmlMarker}</b></p></body></html>`,
  });
  return { textMarker, htmlMarker };
}

async function testChannel(channel: Channel): Promise<void> {
  const client = new TempEmailClient();
  const info = await client.generate({ channel, channelFallback: false });
  if (!info) {
    console.log(JSON.stringify({ channel, step: 'generate-null' }));
    return;
  }

  const marker = `BODY-AUDIT-${channel}-${Date.now()}-${Math.random().toString(36).slice(2, 8)}`;
  const markers = await send(info.email, marker);
  console.log(JSON.stringify({ channel, step: 'sent', email: info.email, marker }));

  for (let round = 1; round <= 24; round++) {
    await sleep(5000);
    const result = await client.getEmails({ retry: { maxRetries: 0, timeout: 15000 } });
    const found = result.emails.find((email) =>
      email.subject.includes(marker) ||
      email.text.includes(markers.textMarker) ||
      email.html.includes(markers.htmlMarker)
    );
    if (!found) {
      console.log(JSON.stringify({ channel, step: 'poll', round, success: result.success, count: result.emails.length }));
      continue;
    }
    console.log(JSON.stringify({
      channel,
      step: 'received',
      round,
      subjectOk: found.subject.includes(marker),
      textOk: found.text.includes(markers.textMarker) || found.text.includes(markers.htmlMarker),
      htmlOk: found.html.includes(markers.htmlMarker) || found.html.includes(markers.textMarker),
      textDerivedFromHtml: !found.text.includes(markers.textMarker) && found.text.includes(markers.htmlMarker),
      htmlDerivedFromText: !found.html.includes(markers.htmlMarker) && found.html.includes(markers.textMarker),
      textLen: found.text.length,
      htmlLen: found.html.length,
    }));
    return;
  }
  console.log(JSON.stringify({ channel, step: 'timeout' }));
}

async function main(): Promise<void> {
  assertSmtpEnv();
  for (const channel of channels) {
    await testChannel(channel);
  }
}

main().catch((err) => {
  console.error(JSON.stringify({ step: 'error', message: err?.message || String(err) }));
  process.exit(1);
});
