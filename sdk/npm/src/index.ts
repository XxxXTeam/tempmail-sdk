import * as tempmail from './providers/tempmail';
import * as linshiEmail from './providers/linshi-email';
import * as tempmailLol from './providers/tempmail-lol';
import * as chatgptOrgUk from './providers/chatgpt-org-uk';
import * as tempmailLa from './providers/tempmail-la';
import * as tempMailIO from './providers/temp-mail-io';
import * as awamail from './providers/awamail';
import * as mailTm from './providers/mail-tm';
import * as dropmail from './providers/dropmail';
import { Channel, EmailInfo, Email, EmailAttachment, GetEmailsResult, GenerateEmailOptions, GetEmailsOptions } from './types';

export { Channel, EmailInfo, Email, EmailAttachment, GetEmailsResult, GenerateEmailOptions, GetEmailsOptions } from './types';
export { normalizeEmail } from './normalize';

const providers = {
  'tempmail': tempmail,
  'linshi-email': linshiEmail,
  'tempmail-lol': tempmailLol,
  'chatgpt-org-uk': chatgptOrgUk,
  'tempmail-la': tempmailLa,
  'temp-mail-io': tempMailIO,
  'awamail': awamail,
  'mail-tm': mailTm,
  'dropmail': dropmail,
};

const allChannels: Channel[] = ['tempmail', 'linshi-email', 'tempmail-lol', 'chatgpt-org-uk', 'tempmail-la', 'temp-mail-io', 'awamail', 'mail-tm', 'dropmail'];

export interface ChannelInfo {
  channel: Channel;
  name: string;
  website: string;
}

const channelInfoMap: Record<Channel, ChannelInfo> = {
  'tempmail': { channel: 'tempmail', name: 'TempMail', website: 'tempmail.ing' },
  'linshi-email': { channel: 'linshi-email', name: '临时邮箱', website: 'linshi-email.com' },
  'tempmail-lol': { channel: 'tempmail-lol', name: 'TempMail LOL', website: 'tempmail.lol' },
  'chatgpt-org-uk': { channel: 'chatgpt-org-uk', name: 'ChatGPT Mail', website: 'mail.chatgpt.org.uk' },
  'tempmail-la': { channel: 'tempmail-la', name: 'TempMail LA', website: 'tempmail.la' },
  'temp-mail-io': { channel: 'temp-mail-io', name: 'Temp Mail IO', website: 'temp-mail.io' },
  'awamail': { channel: 'awamail', name: 'AwaMail', website: 'awamail.com' },
  'mail-tm': { channel: 'mail-tm', name: 'Mail.tm', website: 'mail.tm' },
  'dropmail': { channel: 'dropmail', name: 'DropMail', website: 'dropmail.me' },
};

/**
 * 获取所有支持的渠道列表
 */
export function listChannels(): ChannelInfo[] {
  return allChannels.map(ch => channelInfoMap[ch]);
}

/**
 * 获取指定渠道信息
 */
export function getChannelInfo(channel: Channel): ChannelInfo | undefined {
  return channelInfoMap[channel];
}

export async function generateEmail(options: GenerateEmailOptions = {}): Promise<EmailInfo> {
  const channel = options.channel || allChannels[Math.floor(Math.random() * allChannels.length)];
  
  switch (channel) {
    case 'tempmail':
      return tempmail.generateEmail(options.duration || 30);
    case 'linshi-email':
      return linshiEmail.generateEmail();
    case 'tempmail-lol':
      return tempmailLol.generateEmail(options.domain || null);
    case 'chatgpt-org-uk':
      return chatgptOrgUk.generateEmail();
    case 'tempmail-la':
      return tempmailLa.generateEmail();
    case 'temp-mail-io':
      return tempMailIO.generateEmail();
    case 'awamail':
      return awamail.generateEmail();
    case 'mail-tm':
      return mailTm.generateEmail();
    case 'dropmail':
      return dropmail.generateEmail();
    default:
      throw new Error(`Unknown channel: ${channel}`);
  }
}

export async function getEmails(options: GetEmailsOptions): Promise<GetEmailsResult> {
  const { channel, email, token } = options;
  
  if (!channel) {
    throw new Error('Channel is required');
  }
  if (!email && channel !== 'tempmail-lol') {
    throw new Error('Email is required');
  }

  let emails: Email[] = [];

  switch (channel) {
    case 'tempmail':
      emails = await tempmail.getEmails(email);
      break;
    case 'linshi-email':
      emails = await linshiEmail.getEmails(email);
      break;
    case 'tempmail-lol':
      if (!token) {
        throw new Error('Token is required for tempmail-lol channel');
      }
      emails = await tempmailLol.getEmails(token, email);
      break;
    case 'chatgpt-org-uk':
      emails = await chatgptOrgUk.getEmails(email);
      break;
    case 'tempmail-la':
      emails = await tempmailLa.getEmails(email);
      break;
    case 'temp-mail-io':
      emails = await tempMailIO.getEmails(email);
      break;
    case 'awamail':
      if (!token) {
        throw new Error('Token is required for awamail channel');
      }
      emails = await awamail.getEmails(token, email);
      break;
    case 'mail-tm':
      if (!token) {
        throw new Error('Token is required for mail-tm channel');
      }
      emails = await mailTm.getEmails(token, email);
      break;
    case 'dropmail':
      if (!token) {
        throw new Error('Token is required for dropmail channel');
      }
      emails = await dropmail.getEmails(token, email);
      break;
    default:
      throw new Error(`Unknown channel: ${channel}`);
  }

  return {
    channel,
    email,
    emails,
    success: true,
  };
}

export class TempEmailClient {
  private emailInfo: EmailInfo | null = null;

  async generate(options: GenerateEmailOptions = {}): Promise<EmailInfo> {
    this.emailInfo = await generateEmail(options);
    return this.emailInfo;
  }

  async getEmails(): Promise<GetEmailsResult> {
    if (!this.emailInfo) {
      throw new Error('No email generated. Call generate() first.');
    }

    return getEmails({
      channel: this.emailInfo.channel,
      email: this.emailInfo.email,
      token: this.emailInfo.token,
    });
  }

  getEmailInfo(): EmailInfo | null {
    return this.emailInfo;
  }
}

export default {
  listChannels,
  getChannelInfo,
  generateEmail,
  getEmails,
  TempEmailClient,
};
