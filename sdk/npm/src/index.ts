import * as tempmail from './providers/tempmail';
import * as linshiEmail from './providers/linshi-email';
import * as linshiyou from './providers/linshiyou';
import * as mffac from './providers/mffac';
import * as tempmailLol from './providers/tempmail-lol';
import * as chatgptOrgUk from './providers/chatgpt-org-uk';
import * as tempMailIO from './providers/temp-mail-io';
import * as awamail from './providers/awamail';
import * as temporaryEmailOrg from './providers/temporary-email-org';
import * as mailTm from './providers/mail-tm';
import * as dropmail from './providers/dropmail';
import * as guerrillamail from './providers/guerrillamail';
import * as maildropProvider from './providers/maildrop';
import * as smailPw from './providers/smail-pw';
import * as boomlify from './providers/boomlify';
import * as minmail from './providers/minmail';
import * as vip215 from './providers/vip-215';
import * as anonbox from './providers/anonbox';
import * as fakeLegal from './providers/fake-legal';
import { Channel, EmailInfo, InternalEmailInfo, Email, GetEmailsResult, GenerateEmailOptions, GetEmailsOptions } from './types';
import { withRetry } from './retry';
import { logger } from './logger';
import { setConfig, getConfig } from './config';

export { Channel, EmailInfo, Email, EmailAttachment, GetEmailsResult, GenerateEmailOptions, GetEmailsOptions } from './types';

/**
 * SDK 内部 token 存储
 * 使用 WeakMap 将 EmailInfo 对象映射到对应的 token
 * 用户无法直接访问 token，由 SDK 自动管理
 * @internal
 */
const tokenStore = new WeakMap<EmailInfo, string>();
export { normalizeEmail } from './normalize';
export { withRetry, fetchWithTimeout, RetryOptions } from './retry';
export { LogLevel, LogHandler, setLogLevel, getLogLevel, setLogger, logger } from './logger';
export { SDKConfig, setConfig, getConfig } from './config';
export type { SyntheticBrowserProfile } from './providers/linshi-token';
export {
  deriveLinshiApiPathKey,
  randomSyntheticLinshiKey,
  randomBrowserLikeProfile,
  syntheticVisitorIdFromProfile,
} from './providers/linshi-token';

/** 所有支持的渠道列表，用于随机选择和遍历 */
const allChannels: Channel[] = ['tempmail', 'linshi-email', 'linshiyou', 'mffac', 'tempmail-lol', 'chatgpt-org-uk', 'temp-mail-io', 'awamail', 'temporary-email-org', 'mail-tm', 'dropmail', 'guerrillamail', 'maildrop', 'smail-pw', 'boomlify', 'minmail', 'vip-215', 'anonbox', 'fake-legal'];

/**
 * 渠道信息，包含渠道标识、显示名称和对应网站
 */
export interface ChannelInfo {
  /** 渠道标识 */
  channel: Channel;
  /** 渠道显示名称 */
  name: string;
  /** 对应的临时邮箱服务网站 */
  website: string;
}

/** 渠道信息映射表 */
const channelInfoMap: Record<Channel, ChannelInfo> = {
  'tempmail': { channel: 'tempmail', name: 'TempMail', website: 'tempmail.ing' },
  'linshi-email': { channel: 'linshi-email', name: '临时邮箱', website: 'linshi-email.com' },
  'linshiyou': { channel: 'linshiyou', name: '临时邮', website: 'linshiyou.com' },
  'tempmail-lol': { channel: 'tempmail-lol', name: 'TempMail LOL', website: 'tempmail.lol' },
  'chatgpt-org-uk': { channel: 'chatgpt-org-uk', name: 'ChatGPT Mail', website: 'mail.chatgpt.org.uk' },
  'temp-mail-io': { channel: 'temp-mail-io', name: 'Temp Mail IO', website: 'temp-mail.io' },
  'awamail': { channel: 'awamail', name: 'AwaMail', website: 'awamail.com' },
  'temporary-email-org': { channel: 'temporary-email-org', name: 'Temporary Email', website: 'temporary-email.org' },
  'mail-tm': { channel: 'mail-tm', name: 'Mail.tm', website: 'mail.tm' },
  'dropmail': { channel: 'dropmail', name: 'DropMail', website: 'dropmail.me' },
  'guerrillamail': { channel: 'guerrillamail', name: 'Guerrilla Mail', website: 'guerrillamail.com' },
  'maildrop': { channel: 'maildrop', name: 'Maildrop', website: 'maildrop.cx' },
  'smail-pw': { channel: 'smail-pw', name: 'Smail.pw', website: 'smail.pw' },
  'boomlify': { channel: 'boomlify', name: 'Boomlify', website: 'boomlify.com' },
  'minmail': { channel: 'minmail', name: 'MinMail', website: 'minmail.app' },
  'mffac': { channel: 'mffac', name: 'MFFAC', website: 'mffac.com' },
  'vip-215': { channel: 'vip-215', name: 'VIP 215', website: 'vip.215.im' },
  'anonbox': { channel: 'anonbox', name: 'Anonbox', website: 'anonbox.net' },
  'fake-legal': { channel: 'fake-legal', name: 'Fake Legal', website: 'fake.legal' },
};

/**
 * 获取所有支持的渠道列表
 *
 * @returns 所有渠道的信息数组
 *
 * @example
 * ```ts
 * const channels = listChannels();
 * channels.forEach(ch => console.log(`${ch.name} (${ch.website})`));
 * ```
 */
export function listChannels(): ChannelInfo[] {
  return allChannels.map(ch => channelInfoMap[ch]);
}

/**
 * 获取指定渠道的详细信息
 *
 * @param channel - 渠道标识
 * @returns 渠道信息，不存在时返回 undefined
 */
export function getChannelInfo(channel: Channel): ChannelInfo | undefined {
  return channelInfoMap[channel];
}

/**
 * 创建临时邮箱
 *
 * 错误处理策略：
 * - 指定渠道失败时，默认自动尝试其他可用渠道（打乱顺序逐个尝试）
 * - `channelFallback: false` 且指定了 `channel` 时，仅尝试该渠道，失败即返回 null
 * - 未指定渠道时，打乱全部渠道逐个尝试，直到成功
 * - 所有渠道均不可用时返回 null（不抛出异常）
 *
 * @param options - 创建选项，可指定渠道、有效时长、域名等
 * @returns 邮箱信息，所有渠道均不可用时返回 null
 *
 * @example
 * ```ts
 * const emailInfo = await generateEmail({ channel: 'temp-mail-io' });
 * if (emailInfo) console.log(emailInfo.email);
 * ```
 */
export async function generateEmail(options: GenerateEmailOptions = {}): Promise<EmailInfo | null> {
  /*
   * 构建尝试顺序：
   * - 指定渠道 → 优先尝试该渠道，失败后随机尝试其他渠道
   * - 未指定 → 打乱全部渠道逐个尝试
   */
  const allowFallback = options.channelFallback !== false;
  const tryOrder = buildChannelOrder(options.channel, allowFallback);

  for (const ch of tryOrder) {
    logger.info(`创建临时邮箱, 渠道: ${ch}`);
    try {
      const internal: InternalEmailInfo = await withRetry(() => generateEmailOnce(ch, options), options.retry);
      logger.info(`邮箱创建成功: ${internal.email} (渠道: ${ch})`);

      /* 将 token 存入内部存储，不暴露给用户 */
      const publicInfo: EmailInfo = {
        channel: internal.channel,
        email: internal.email,
        expiresAt: internal.expiresAt,
        createdAt: internal.createdAt,
      };
      if (internal.token) {
        tokenStore.set(publicInfo, internal.token);
      }
      return publicInfo;
    } catch (err: any) {
      logger.warn(`渠道 ${ch} 不可用: ${err.message || err}，尝试下一个渠道`);
    }
  }

  logger.error('所有渠道均不可用，创建邮箱失败');
  return null;
}

/**
 * 构建渠道尝试顺序
 * 指定渠道时优先尝试该渠道，其余渠道打乱追加
 * 未指定时打乱全部渠道
 */
function buildChannelOrder(preferred?: Channel, allowFallback = true): Channel[] {
  const shuffled = [...allChannels].sort(() => Math.random() - 0.5);
  if (!preferred) {
    return shuffled;
  }
  if (!allowFallback) {
    return [preferred];
  }
  const rest = shuffled.filter(ch => ch !== preferred);
  return [preferred, ...rest];
}

/**
 * 单次创建邮箱（不含重试逻辑）
 * 根据渠道类型分发到对应的 provider 实现
 */
async function generateEmailOnce(channel: Channel, options: GenerateEmailOptions): Promise<InternalEmailInfo> {
  switch (channel) {
    case 'tempmail':
      return tempmail.generateEmail(options.duration || 30);
    case 'linshi-email':
      return linshiEmail.generateEmail();
    case 'linshiyou':
      return linshiyou.generateEmail();
    case 'tempmail-lol':
      return tempmailLol.generateEmail(options.domain || null);
    case 'chatgpt-org-uk':
      return chatgptOrgUk.generateEmail();
    case 'temp-mail-io':
      return tempMailIO.generateEmail();
    case 'awamail':
      return awamail.generateEmail();
    case 'temporary-email-org':
      return temporaryEmailOrg.generateEmail();
    case 'mail-tm':
      return mailTm.generateEmail();
    case 'dropmail':
      return dropmail.generateEmail();
    case 'guerrillamail':
      return guerrillamail.generateEmail();
    case 'maildrop':
      return maildropProvider.generateEmail(options.domain ?? null);
    case 'smail-pw':
      return smailPw.generateEmail();
    case 'boomlify':
      return boomlify.generateEmail();
    case 'mffac':
      return mffac.generateEmail();
    case 'minmail':
      return minmail.generateEmail();
    case 'vip-215':
      return vip215.generateEmail();
    case 'anonbox':
      return anonbox.generateEmail();
    case 'fake-legal':
      return fakeLegal.generateEmail(options.domain ?? null);
    default:
      throw new Error(`Unknown channel: ${channel}`);
  }
}

/**
 * 获取邮件列表
 * Channel/Email/Token 等由 SDK 从 EmailInfo 中自动获取，用户无需手动传递
 *
 * 错误处理策略：
 * - 网络错误、超时、429、服务端 5xx 错误 → 自动重试（默认 2 次）
 * - 重试耗尽后返回 { success: false, emails: [] }，不抛异常
 * - 参数校验错误（缺少 EmailInfo）直接抛出
 *
 * @param info - GenerateEmail() 返回的邮箱信息
 * @param options - 可选配置（重试等）
 * @returns 邮件结果，包含 success 标记和邮件列表
 *
 * @example
 * ```ts
 * const info = await generateEmail({ channel: 'mail-tm' });
 * const result = await getEmails(info);
 * if (result.success && result.emails.length > 0) {
 *   console.log('收到邮件:', result.emails[0].subject);
 * }
 * ```
 */
export async function getEmails(info: EmailInfo, options?: GetEmailsOptions): Promise<GetEmailsResult> {
  if (!info) {
    throw new Error('EmailInfo is required, call generateEmail() first');
  }

  const { channel, email } = info;
  const token = tokenStore.get(info);

  if (!channel) {
    throw new Error('Channel is required');
  }
  if (!email && channel !== 'tempmail-lol') {
    throw new Error('Email is required');
  }

  logger.debug(`获取邮件, 渠道: ${channel}, 邮箱: ${email}`);
  try {
    const emails = await withRetry(() => getEmailsOnce(channel, email, token), options?.retry);
    if (emails.length > 0) {
      logger.info(`获取到 ${emails.length} 封邮件, 渠道: ${channel}`);
    } else {
      logger.debug(`暂无邮件, 渠道: ${channel}`);
    }
    return { channel, email, emails, success: true };
  } catch (err: any) {
    /*
     * 重试耗尽后仍然失败 → 返回空结果而非抛异常
     * 这样调用方在轮询场景下不会因为一次网络波动而中断整个流程
     */
    logger.error(`获取邮件失败, 渠道: ${channel}, 错误: ${err.message || err}`);
    return { channel, email, emails: [], success: false };
  }
}

/**
 * 单次获取邮件（不含重试逻辑）
 * 根据渠道类型分发到对应的 provider 实现，并校验必需的 token 参数
 */
async function getEmailsOnce(channel: Channel, email: string, token?: string): Promise<Email[]> {
  switch (channel) {
    case 'tempmail':
      return tempmail.getEmails(email);
    case 'linshi-email':
      if (!token) throw new Error('internal error: token missing for linshi-email');
      return linshiEmail.getEmails(email, token);
    case 'linshiyou':
      if (!token) throw new Error('internal error: token missing for linshiyou');
      return linshiyou.getEmails(token, email);
    case 'tempmail-lol':
      if (!token) throw new Error('internal error: token missing for tempmail-lol');
      return tempmailLol.getEmails(token, email);
    case 'chatgpt-org-uk':
      if (!token) throw new Error('internal error: token missing for chatgpt-org-uk');
      return chatgptOrgUk.getEmails(token, email);
    case 'temp-mail-io':
      return tempMailIO.getEmails(email);
    case 'awamail':
      if (!token) throw new Error('internal error: token missing for awamail');
      return awamail.getEmails(token, email);
    case 'temporary-email-org':
      if (!token) throw new Error('internal error: token missing for temporary-email-org');
      return temporaryEmailOrg.getEmails(email, token);
    case 'mail-tm':
      if (!token) throw new Error('internal error: token missing for mail-tm');
      return mailTm.getEmails(token, email);
    case 'dropmail':
      if (!token) throw new Error('internal error: token missing for dropmail');
      return dropmail.getEmails(token, email);
    case 'guerrillamail':
      if (!token) throw new Error('internal error: token missing for guerrillamail');
      return guerrillamail.getEmails(token, email);
    case 'maildrop':
      if (!token) throw new Error('internal error: token missing for maildrop');
      return maildropProvider.getEmails(token, email);
    case 'smail-pw':
      if (!token) throw new Error('internal error: token missing for smail-pw');
      return smailPw.getEmails(token, email);
    case 'boomlify':
      return boomlify.getEmails(email);
    case 'mffac':
      return mffac.getEmails(email, token);
    case 'minmail':
      if (!token) throw new Error('internal error: token missing for minmail');
      return minmail.getEmails(email, token);
    case 'vip-215':
      if (!token) throw new Error('internal error: token missing for vip-215');
      return vip215.getEmails(token, email);
    case 'anonbox':
      if (!token) throw new Error('internal error: token missing for anonbox');
      return anonbox.getEmails(token, email);
    case 'fake-legal':
      return fakeLegal.getEmails(email);
    default:
      throw new Error(`Unknown channel: ${channel}`);
  }
}

/**
 * 临时邮箱客户端
 * 封装了邮箱创建和邮件获取的完整流程，自动管理邮箱信息和认证令牌
 *
 * @example
 * ```ts
 * const client = new TempEmailClient();
 * const emailInfo = await client.generate({ channel: 'mail-tm' });
 * console.log('邮箱:', emailInfo.email);
 *
 * // 轮询获取邮件
 * const result = await client.getEmails();
 * if (result.success) {
 *   console.log('邮件数:', result.emails.length);
 * }
 * ```
 */
export class TempEmailClient {
  private emailInfo: EmailInfo | null = null;

  /**
   * 创建临时邮箱并缓存邮箱信息
   * 后续调用 getEmails() 时自动使用此邮箱的渠道、地址和令牌
   * 所有渠道均不可用时返回 null
   */
  async generate(options: GenerateEmailOptions = {}): Promise<EmailInfo | null> {
    this.emailInfo = await generateEmail(options);
    return this.emailInfo;
  }

  /**
   * 获取当前邮箱的邮件列表
   * 必须先调用 generate() 创建邮箱
   *
   * @throws 未调用 generate() 时抛出异常
   */
  async getEmails(options?: GetEmailsOptions): Promise<GetEmailsResult> {
    if (!this.emailInfo) {
      throw new Error('No email generated. Call generate() first.');
    }

    return getEmails(this.emailInfo, options);
  }

  /**
   * 获取当前缓存的邮箱信息
   * 未调用 generate() 时返回 null
   */
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
  setConfig,
  getConfig,
};
