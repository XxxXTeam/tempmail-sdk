import { TempEmailClient, setConfig } from "../src";

setConfig({ telemetryEnabled: false });

(async () => {
  const channel = "catchmail-mailistry";
  console.log(`Testing ${channel} generate...`);
  const client = new TempEmailClient();
  const info = await client.generate({ channel, channelFallback: false });
  if (!info) {
    console.log("generate failed");
    return;
  }
  console.log("channel:", info.channel);
  console.log("email:", info.email);

  console.log(`\nTesting ${channel} getEmails...`);
  const result = await client.getEmails();
  console.log("success:", result.success);
  console.log("emails:", result.emails.length);
})().catch((e) => console.error(e.message));
