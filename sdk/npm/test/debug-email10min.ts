import * as e10 from "../src/providers/email10min";
import { sendSmtp } from "./smtp-env";

(async () => {
  const r = await e10.generateEmail();
  console.log("email:", r.email, "token:", !!r.token);
  const mk = "dbg-" + Date.now();
  await sendSmtp(
    r.email,
    `Debug [${mk}]`,
    `Text body ${mk}`,
    `<p>HTML <b>${mk}</b></p>`,
  );
  console.log("sent, waiting...");
  for (let i = 0; i < 6; i++) {
    await new Promise((r) => setTimeout(r, 5000));
    const emails = await e10.getEmails(r.email, r.token!);
    console.log(`poll ${i + 1}: ${emails.length} emails`);
    for (const e of emails) {
      console.log(
        JSON.stringify({
          id: e.id,
          subject: e.subject,
          from: e.from,
          textLen: e.text?.length,
          htmlLen: e.html?.length,
          textPre: e.text?.substring(0, 50),
          htmlPre: e.html?.substring(0, 50),
        }),
      );
    }
    if (emails.length > 0) break;
  }
})().catch((e) => console.error(e.message));
