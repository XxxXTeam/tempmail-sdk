import { generateEmail, getEmails, TempEmailClient, Channel, setConfig } from '../src';

setConfig({ telemetryEnabled: false });

async function testChannel(channel: Channel) {
  console.log(`\n测试渠道: ${channel}`);
  try {
    const client = new TempEmailClient();
    const info = await client.generate({ channel, channelFallback: false });
    if (!info) {
      console.log('  ❌ 生成邮箱失败: 返回 null');
      return;
    }
    console.log(`  ✅ 生成邮箱: ${info.email}`);
    const result = await client.getEmails();
    console.log(`  ${result.success ? '✅' : '❌'} 获取邮件: success=${result.success}, count=${result.emails.length}`);
  } catch (e: any) {
    console.log(`  ❌ 异常: ${e.message}`);
  }
}

async function main() {
  const channels: Channel[] = ['moakt', 'temporary-email-org', 'tempmail-cn', 'minmail'];
  for (const ch of channels) {
    await testChannel(ch);
  }
}

main().catch(console.error);
