import { generateEmail, getEmails, TempEmailClient, Channel } from '../src';

async function testGenerateEmail() {
  console.log('=== Test Generate Email ===\n');

  // Test each channel
  const channels: Channel[] = ['tempmail', 'linshi-email', 'tempmail-lol', 'chatgpt-org-uk', 'tempmail-la', 'temp-mail-io', 'awamail', 'mail-tm', 'dropmail'];
  
  for (const channel of channels) {
    try {
      console.log(`Testing channel: ${channel}`);
      const emailInfo = await generateEmail({ channel });
      console.log(`  Channel: ${emailInfo.channel}`);
      console.log(`  Email: ${emailInfo.email}`);
      if (emailInfo.token) console.log(`  Token: ${emailInfo.token}`);
      if (emailInfo.expiresAt) console.log(`  Expires: ${emailInfo.expiresAt}`);
      if (emailInfo.createdAt) console.log(`  Created: ${emailInfo.createdAt}`);
      console.log();
    } catch (error) {
      console.error(`  Error: ${error}`);
      console.log();
    }
  }
}

async function testGetEmails() {
  console.log('=== Test Get Emails ===\n');

  // Generate and check emails using client
  const client = new TempEmailClient();
  
  try {
    const emailInfo = await client.generate({ channel: 'tempmail' });
    console.log(`Generated: ${emailInfo.channel} - ${emailInfo.email}`);
    
    const result = await client.getEmails();
    console.log(`Emails count: ${result.emails.length}`);

    // 展示标准化的邮件格式
    for (const email of result.emails) {
      console.log(`  ID: ${email.id}`);
      console.log(`  From: ${email.from}`);
      console.log(`  To: ${email.to}`);
      console.log(`  Subject: ${email.subject}`);
      console.log(`  Date: ${email.date}`);
      console.log(`  IsRead: ${email.isRead}`);
      console.log(`  Text: ${email.text.substring(0, 100)}`);
      console.log(`  HTML: ${email.html.substring(0, 100)}`);
      console.log(`  Attachments: ${email.attachments.length}`);
      console.log();
    }
    console.log();
  } catch (error) {
    console.error(`Error: ${error}`);
  }
}

async function main() {
  await testGenerateEmail();
  await testGetEmails();
}

main().catch(console.error);
