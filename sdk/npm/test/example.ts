import { generateEmail, getEmails, TempEmailClient, Channel } from '../src';

async function testGenerateEmail() {
  console.log('=== Test Generate Email ===\n');

  // Test each channel
  const channels: Channel[] = ['tempmail', 'linshi-email', 'tempmail-lol', 'chatgpt-org-uk'];
  
  for (const channel of channels) {
    try {
      console.log(`Testing channel: ${channel}`);
      const emailInfo = await generateEmail({ channel });
      console.log(`  Channel: ${emailInfo.channel}`);
      console.log(`  Email: ${emailInfo.email}`);
      if (emailInfo.token) console.log(`  Token: ${emailInfo.token}`);
      if (emailInfo.expiresAt) console.log(`  Expires: ${emailInfo.expiresAt}`);
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
