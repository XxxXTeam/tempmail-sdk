/**
 * Demo: 获取指定渠道邮箱，轮询获取邮件
 * 
 * 使用方法:
 * npx ts-node demo/poll-emails.ts
 */

import * as readline from 'readline';
import { generateEmail, getEmails, listChannels, getChannelInfo, Channel, EmailInfo, ChannelInfo, Email } from '../src';

// 配置
const POLL_INTERVAL = 5000;           // 轮询间隔（毫秒）
const MAX_POLL_COUNT = 60;            // 最大轮询次数

function printJson(label: string, data: any): void {
  console.log(`\n${label}:`);
  console.log(JSON.stringify(data, null, 2));
}

function sleep(ms: number): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, ms));
}

function question(prompt: string): Promise<string> {
  const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout,
  });
  return new Promise((resolve) => {
    rl.question(prompt, (answer: string) => {
      rl.close();
      resolve(answer.trim());
    });
  });
}

async function selectChannel(channels: ChannelInfo[]): Promise<Channel> {
  console.log('\n请选择渠道:');
  channels.forEach((ch, index) => {
    console.log(`  [${index + 1}] ${ch.channel} - ${ch.name} (${ch.website})`);
  });
  
  while (true) {
    const input = await question('\n请输入渠道编号 (1-' + channels.length + '): ');
    const num = parseInt(input, 10);
    
    if (num >= 1 && num <= channels.length) {
      return channels[num - 1].channel;
    }
    console.log('❌ 无效的编号，请重新输入');
  }
}

function printEmail(email: Email, index: number): void {
  console.log(`\n邮件 #${index + 1}`);
  console.log('━'.repeat(50));
  console.log(`ID: ${email.id || '无'}`);
  console.log(`发件人: ${email.from || '未知'}`);
  console.log(`收件人: ${email.to || '未知'}`);
  console.log(`主题: ${email.subject || '无主题'}`);
  console.log(`时间: ${email.date || '未知'}`);
  console.log(`已读: ${email.isRead ? '是' : '否'}`);
  if (email.text) {
    const preview = email.text.substring(0, 200);
    console.log(`内容: ${preview}`);
  }
  if (email.html) {
    const htmlPreview = email.html.replace(/<[^>]*>/g, '').substring(0, 100);
    console.log(`HTML: ${htmlPreview}...`);
  }
  if (email.attachments.length > 0) {
    console.log(`附件: ${email.attachments.map(a => a.filename).join(', ')}`);
  }
  console.log('━'.repeat(50));
}

async function pollEmails(emailInfo: EmailInfo): Promise<void> {
  console.log(`\n开始轮询邮件...（每 ${POLL_INTERVAL / 1000} 秒检查一次）`);
  console.log('按 Ctrl+C 停止轮询\n');

  let pollCount = 0;

  while (pollCount < MAX_POLL_COUNT) {
    pollCount++;
    
    try {
      const result = await getEmails(emailInfo);

      const timestamp = new Date().toLocaleTimeString();
      
      if (result.emails.length > 0) {
        console.log(`\n[${timestamp}] 🎉 收到 ${result.emails.length} 封邮件！\n`);
        
        // 打印原始返回数据
        printJson('返回数据 (GetEmailsResult)', result);

        for (let i = 0; i < result.emails.length; i++) {
          printEmail(result.emails[i], i);
        }
        
        // 收到邮件后可以选择继续轮询或退出
        // break;
      } else {
        process.stdout.write(`\r[${timestamp}] 第 ${pollCount}/${MAX_POLL_COUNT} 次检查，暂无新邮件...`);
      }
    } catch (error) {
      console.error(`\n轮询出错: ${error}`);
    }

    await sleep(POLL_INTERVAL);
  }

  console.log('\n\n已达到最大轮询次数，停止轮询');
}

async function main(): Promise<void> {
  console.log('═'.repeat(50));
  console.log('  临时邮箱 Demo - 获取邮箱并轮询邮件');
  console.log('═'.repeat(50));

  // 1. 列出所有支持的渠道
  console.log('\n[1] 列出所有支持的渠道...');
  const channels = listChannels();
  printJson('支持的渠道列表', channels);

  // 2. 让用户选择渠道
  console.log('\n[2] 选择渠道...');
  const selectedChannel = await selectChannel(channels);
  
  // 3. 获取指定渠道信息
  console.log(`\n[3] 获取渠道 "${selectedChannel}" 信息...`);
  const channelInfo = getChannelInfo(selectedChannel);
  printJson('渠道信息', channelInfo);

  // 4. 从指定渠道获取邮箱
  console.log(`\n[4] 从 ${selectedChannel} 渠道获取临时邮箱...`);
  
  try {
    const emailInfo = await generateEmail({ channel: selectedChannel });
    
    if (!emailInfo) {
      console.error('\n❌ 所有渠道均不可用');
      process.exit(1);
      return;
    }

    console.log('\n✅ 获取邮箱成功！');
    printJson('返回数据 (EmailInfo)', emailInfo);
    
    console.log('\n📋 邮箱信息:');
    console.log(`   渠道: ${emailInfo.channel}`);
    console.log(`   邮箱: ${emailInfo.email}`);
    if (emailInfo.expiresAt) {
      console.log(`   过期时间: ${emailInfo.expiresAt}`);
    }
    if (emailInfo.createdAt) {
      console.log(`   创建时间: ${emailInfo.createdAt}`);
    }

    console.log('\n📧 请发送邮件到以上邮箱地址进行测试');
    
    // 5. 轮询获取邮件
    await pollEmails(emailInfo);

  } catch (error) {
    console.error(`\n❌ 获取邮箱失败: ${error}`);
    process.exit(1);
  }
}

main().catch(console.error);
