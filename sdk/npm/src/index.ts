import * as tempmail from './providers/tempmail';
import * as tempmailCn from './providers/tempmail-cn';
import * as tmpmails from './providers/tmpmails';
import * as taEasy from './providers/ta-easy';
import * as linshiyou from './providers/linshiyou';
import * as mffac from './providers/mffac';
import * as tempmailLol from './providers/tempmail-lol';
import * as chatgptOrgUk from './providers/chatgpt-org-uk';
import * as awamail from './providers/awamail';
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
import * as moakt from './providers/moakt';
import * as tenminuteOne from './providers/10minute-one';
import * as etempmail from './providers/etempmail';
import * as twentyFourMailChacuo from './providers/24mail-chacuo';
import * as email10min from './providers/email10min';
import * as mjjCm from './providers/mjj-cm';
import * as mailXiuvi from './providers/mail-xiuvi';
import * as linshiCo from './providers/linshi-co';
import * as harakirimail from './providers/harakirimail';
import * as tempmailPlus from './providers/tempmail-plus';
import * as mailGw from './providers/mail-gw';
import * as tempmailLolV2 from './providers/tempmail-lol-v2';
import { sharklasers, grrla, guerrillainfoMirror } from './providers/guerrillamail-mirrors';
import { Channel, EmailInfo, InternalEmailInfo, Email, GetEmailsResult, GenerateEmailOptions, GetEmailsOptions } from './types';
import { withRetry, withRetryWithAttempts } from './retry';
import { reportTelemetry } from './telemetry';
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
export { withRetry, withRetryWithAttempts, fetchWithTimeout, RetryOptions } from './retry';
export type { RetryWithAttemptsResult, FetchWithTimeoutOptions } from './retry';
export { LogLevel, LogHandler, setLogLevel, getLogLevel, setLogger, logger } from './logger';
export { SDKConfig, setConfig, getConfig } from './config';
export { getSdkVersion } from './version';

/** 所有支持的渠道列表，用于随机选择和遍历 */
const allChannels: Channel[] = ['tempmail', 'tempmail-cn', 'tmpmails', 'ta-easy', '10minute-one', 'linshiyou', 'mffac', 'tempmail-lol', 'chatgpt-org-uk', 'awamail', 'mail-tm', 'dropmail', 'guerrillamail', 'maildrop', 'smail-pw', 'boomlify', 'minmail', 'vip-215', 'anonbox', 'fake-legal', 'moakt', 'etempmail', '24mail-chacuo', 'email10min', 'mjj-cm', 'mail-xiuvi', 'linshi-co', 'harakirimail', 'tempmail-plus', 'mail-gw', 'tempmail-lol-v2', 'sharklasers', 'grr-la', 'guerrillamail-info'];

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
  'tempmail-cn': { channel: 'tempmail-cn', name: 'TempMail CN', website: 'tempmail.cn' },
  'tmpmails': { channel: 'tmpmails', name: 'TmpMails', website: 'tmpmails.com' },
  'ta-easy': { channel: 'ta-easy', name: 'TA Easy', website: 'ta-easy.com' },
  '10minute-one': { channel: '10minute-one', name: '10 Minute Email', website: '10minutemail.one' },
  'linshiyou': { channel: 'linshiyou', name: '临时邮', website: 'linshiyou.com' },
  'tempmail-lol': { channel: 'tempmail-lol', name: 'TempMail LOL', website: 'tempmail.lol' },
  'chatgpt-org-uk': { channel: 'chatgpt-org-uk', name: 'ChatGPT Mail', website: 'mail.chatgpt.org.uk' },
  'awamail': { channel: 'awamail', name: 'AwaMail', website: 'awamail.com' },
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
  'moakt': { channel: 'moakt', name: 'Moakt', website: 'moakt.com' },
  'etempmail': { channel: 'etempmail', name: 'eTempMail', website: 'etempmail.com' },
  '24mail-chacuo': { channel: '24mail-chacuo', name: '24Mail Chacuo', website: '24mail.chacuo.net' },
  'email10min': { channel: 'email10min', name: 'Email10Min', website: 'email10min.com' },
  'mjj-cm': { channel: 'mjj-cm', name: 'MJJ Mail', website: 'mjj.cm' },
  'mail-xiuvi': { channel: 'mail-xiuvi', name: 'Xiuvi Mail', website: 'mail.xiuvi.cn' },
  'linshi-co': { channel: 'linshi-co', name: '临时Co', website: 'linshi.co' },
  'harakirimail': { channel: 'harakirimail', name: 'HarakiriMail', website: 'harakirimail.com' },
  'tempmail-plus': { channel: 'tempmail-plus', name: 'TempMail Plus', website: 'tempmail.plus' },
  'mail-gw': { channel: 'mail-gw', name: 'Mail.gw', website: 'mail.gw' },
  'tempmail-lol-v2': { channel: 'tempmail-lol-v2', name: 'TempMail LOL V2', website: 'tempmail.lol' },
  'sharklasers': { channel: 'sharklasers', name: 'SharkLasers', website: 'sharklasers.com' },
  'grr-la': { channel: 'grr-la', name: 'Grr.la', website: 'grr.la' },
  'guerrillamail-info': { channel: 'guerrillamail-info', name: 'GuerrillaMail Info', website: 'guerrillamail.info' },
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

  let channelsTried = 0;
  let lastErr = '';
  for (const ch of tryOrder) {
    channelsTried += 1;
    logger.info(`创建临时邮箱, 渠道: ${ch}`);
    const r = await withRetryWithAttempts(() => generateEmailOnce(ch, options), options.retry);
    if (r.ok) {
      const internal = r.value;
      logger.info(`邮箱创建成功: ${internal.email} (渠道: ${ch})`);
      reportTelemetry('generate_email', ch, true, r.attempts, channelsTried, '');

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
    }
    const msg = (r.error as any)?.message || String(r.error);
    lastErr = msg;
    logger.warn(`渠道 ${ch} 不可用: ${msg}，尝试下一个渠道`);
  }

  logger.error('所有渠道均不可用，创建邮箱失败');
  reportTelemetry('generate_email', '', false, 0, channelsTried, lastErr);
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
    case 'tempmail-cn':
      return tempmailCn.generateEmail(options.domain ?? null);
    case 'tmpmails':
      return tmpmails.generateEmail(options.domain ?? null);
    case 'ta-easy':
      return taEasy.generateEmail();
    case '10minute-one':
      return tenminuteOne.generateEmail(options.domain ?? null);
    case 'linshiyou':
      return linshiyou.generateEmail();
    case 'tempmail-lol':
      return tempmailLol.generateEmail(options.domain || null);
    case 'chatgpt-org-uk':
      return chatgptOrgUk.generateEmail();
    case 'awamail':
      return awamail.generateEmail();
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
    case 'moakt':
      return moakt.generateEmail(options.domain ?? null);
    case 'etempmail':
      return etempmail.generateEmail();
    case '24mail-chacuo':
      return twentyFourMailChacuo.generateEmail();
    case 'email10min':
      return email10min.generateEmail();
    case 'mjj-cm':
      return mjjCm.generateEmail();
    case 'mail-xiuvi':
      return mailXiuvi.generateEmail();
    case 'linshi-co':
      return linshiCo.generateEmail();
    case 'harakirimail':
      return harakirimail.generateEmail();
    case 'tempmail-plus':
      return tempmailPlus.generateEmail();
    case 'mail-gw':
      return mailGw.generateEmail();
    case 'tempmail-lol-v2':
      return tempmailLolV2.generateEmail();
    case 'sharklasers':
      return sharklasers.generateEmail();
    case 'grr-la':
      return grrla.generateEmail();
    case 'guerrillamail-info':
      return guerrillainfoMirror.generateEmail();
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
    reportTelemetry('get_emails', '', false, 0, 0, 'EmailInfo is required, call generateEmail() first');
    throw new Error('EmailInfo is required, call generateEmail() first');
  }

  const { channel, email } = info;
  const token = tokenStore.get(info);

  if (!channel) {
    reportTelemetry('get_emails', '', false, 0, 0, 'Channel is required');
    throw new Error('Channel is required');
  }
  if (!email && channel !== 'tempmail-lol') {
    reportTelemetry('get_emails', channel, false, 0, 0, 'Email is required');
    throw new Error('Email is required');
  }

  logger.debug(`获取邮件, 渠道: ${channel}, 邮箱: ${email}`);
  const gr = await withRetryWithAttempts(() => getEmailsOnce(channel, email, token), options?.retry);
  if (gr.ok) {
    const emails = gr.value;
    if (emails.length > 0) {
      logger.info(`获取到 ${emails.length} 封邮件, 渠道: ${channel}`);
    } else {
      logger.debug(`暂无邮件, 渠道: ${channel}`);
    }
    reportTelemetry('get_emails', channel, true, gr.attempts, 0, '');
    return { channel, email, emails, success: true };
  }
  const err = gr.error as any;
  const errMsg = err?.message || String(err);
  /*
   * 重试耗尽后仍然失败 → 返回空结果而非抛异常
   * 这样调用方在轮询场景下不会因为一次网络波动而中断整个流程
   */
  logger.error(`获取邮件失败, 渠道: ${channel}, 错误: ${errMsg}`);
  reportTelemetry('get_emails', channel, false, gr.attempts, 0, errMsg);
  return { channel, email, emails: [], success: false };
}

/**
 * 单次获取邮件（不含重试逻辑）
 * 根据渠道类型分发到对应的 provider 实现，并校验必需的 token 参数
 */
async function getEmailsOnce(channel: Channel, email: string, token?: string): Promise<Email[]> {
  switch (channel) {
    case 'tempmail':
      return tempmail.getEmails(email);
    case 'tempmail-cn':
      return tempmailCn.getEmails(email);
    case 'tmpmails':
      if (!token) throw new Error('internal error: token missing for tmpmails');
      return tmpmails.getEmails(email, token);
    case 'ta-easy':
      if (!token) throw new Error('internal error: token missing for ta-easy');
      return taEasy.getEmails(email, token);
    case '10minute-one':
      if (!token) throw new Error('internal error: token missing for 10minute-one');
      return tenminuteOne.getEmails(email, token);
    case 'linshiyou':
      if (!token) throw new Error('internal error: token missing for linshiyou');
      return linshiyou.getEmails(token, email);
    case 'tempmail-lol':
      if (!token) throw new Error('internal error: token missing for tempmail-lol');
      return tempmailLol.getEmails(token, email);
    case 'chatgpt-org-uk':
      if (!token) throw new Error('internal error: token missing for chatgpt-org-uk');
      return chatgptOrgUk.getEmails(token, email);
    case 'awamail':
      if (!token) throw new Error('internal error: token missing for awamail');
      return awamail.getEmails(token, email);
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
    case 'moakt':
      if (!token) throw new Error('internal error: token missing for moakt');
      return moakt.getEmails(email, token);
    case 'etempmail':
      if (!token) throw new Error('internal error: token missing for etempmail');
      return etempmail.getEmails(email, token);
    case '24mail-chacuo':
      return twentyFourMailChacuo.getEmails(email, token);
    case 'email10min':
      if (!token) throw new Error('internal error: token missing for email10min');
      return email10min.getEmails(email, token);
    case 'mjj-cm':
      return mjjCm.getEmails(email);
    case 'mail-xiuvi':
      return mailXiuvi.getEmails(email);
    case 'linshi-co':
      return linshiCo.getEmails(email);
    case 'harakirimail':
      return harakirimail.getEmails(email);
    case 'tempmail-plus':
      return tempmailPlus.getEmails(email);
    case 'mail-gw':
      if (!token) throw new Error('internal error: token missing for mail-gw');
      return mailGw.getEmails(token, email);
    case 'tempmail-lol-v2':
      if (!token) throw new Error('internal error: token missing for tempmail-lol-v2');
      return tempmailLolV2.getEmails(token, email);
    case 'sharklasers':
      if (!token) throw new Error('internal error: token missing for sharklasers');
      return sharklasers.getEmails(token, email);
    case 'grr-la':
      if (!token) throw new Error('internal error: token missing for grr-la');
      return grrla.getEmails(token, email);
    case 'guerrillamail-info':
      if (!token) throw new Error('internal error: token missing for guerrillamail-info');
      return guerrillainfoMirror.getEmails(token, email);
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
      reportTelemetry('get_emails', '', false, 0, 0, 'No email generated. Call generate() first.');
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
