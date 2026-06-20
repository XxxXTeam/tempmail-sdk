import * as tempmail from './providers/tempmail';
import * as tempmailCn from './providers/tempmail-cn';
import * as taEasy from './providers/ta-easy';
import * as linshiyou from './providers/linshiyou';
import * as mffac from './providers/mffac';
import * as tempmailLol from './providers/tempmail-lol';
import * as chatgptOrgUk from './providers/chatgpt-org-uk';
import * as tempMailIo from './providers/temp-mail-io';
import * as mailCx from './providers/mail-cx';
import * as catchmail from './providers/catchmail';
import * as mailforspam from './providers/mailforspam';
import * as tempmailo from './providers/tempmailo';
import * as tempmailc from './providers/tempmailc';
import * as mailnesia from './providers/mailnesia';
import * as throwawaymail from './providers/throwawaymail';
import * as inboxkitten from './providers/inboxkitten';
import * as getnada from './providers/getnada';
import * as mail123 from './providers/mail123';
import * as oneSecMail from './providers/one-sec-mail';
import * as fakemail from './providers/fakemail';
import * as openinbox from './providers/openinbox';
import * as inboxes from './providers/inboxes';
import * as uncorreotemporal from './providers/uncorreotemporal';
import * as awamail from './providers/awamail';
import * as mailTm from './providers/mail-tm';
import * as dropmail from './providers/dropmail';
import * as guerrillamail from './providers/guerrillamail';
import * as maildropProvider from './providers/maildrop';
import * as smailPw from './providers/smail-pw';
import * as vip215 from './providers/vip-215';
import * as fakeLegal from './providers/fake-legal';
import * as moakt from './providers/moakt';
import * as tenminuteOne from './providers/10minute-one';
import * as email10min from './providers/email10min';
import * as mjjCm from './providers/mjj-cm';
import * as linshiCo from './providers/linshi-co';
import * as harakirimail from './providers/harakirimail';
import * as tempmailPlus from './providers/tempmail-plus';
import * as mailGw from './providers/mail-gw';
import * as tempmailLolV2 from './providers/tempmail-lol-v2';
import {
  sharklasers,
  sharklasersCom,
  grrla,
  grrlaCom,
  guerrillainfoMirror,
  spam4meMirror,
  guerrillamailNetMirror,
  guerrillamailOrgMirror,
  guerrillamailBlockMirror,
  guerrillamailComMirror,
  guerrillamailComWwwMirror,
} from './providers/guerrillamail-mirrors';
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
const allChannels: Channel[] = ['tempmail', 'tempmail-cn', 'ta-easy', '10minute-one', 'linshiyou', 'mffac', 'tempmail-lol', 'chatgpt-org-uk', 'temp-mail-io', 'mail-cx', 'catchmail', 'catchmail-mailistry', 'catchmail-zeppost', 'mailforspam', 'mailforspam-tempmail-io', 'mailforspam-disposable', 'tempmailo', 'tempmailc', 'mailnesia', 'throwawaymail', 'inboxkitten', 'getnada', 'mail123', '1sec-mail', 'fakemail', 'openinbox', 'inboxes', 'uncorreotemporal', 'awamail', 'mail-tm', 'dropmail', 'guerrillamail', 'guerrillamail-com', 'maildrop', 'smail-pw', 'vip-215', 'fake-legal', 'moakt', 'email10min', 'mjj-cm', 'linshi-co', 'harakirimail', 'tempmail-plus', 'mail-gw', 'tempmail-lol-v2', 'sharklasers', 'sharklasers-com', 'grr-la', 'grr-la-com', 'guerrillamail-info', 'spam4me', 'guerrillamail-net', 'guerrillamail-org', 'guerrillamailblock', 'guerrillamail-com-www'];

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
  'ta-easy': { channel: 'ta-easy', name: 'TA Easy', website: 'ta-easy.com' },
  '10minute-one': { channel: '10minute-one', name: '10 Minute Email', website: '10minutemail.one' },
  'linshiyou': { channel: 'linshiyou', name: '临时邮', website: 'linshiyou.com' },
  'mffac': { channel: 'mffac', name: 'MFFAC', website: 'mffac.com' },
  'tempmail-lol': { channel: 'tempmail-lol', name: 'TempMail LOL', website: 'tempmail.lol' },
  'chatgpt-org-uk': { channel: 'chatgpt-org-uk', name: 'ChatGPT Mail', website: 'mail.chatgpt.org.uk' },
  'temp-mail-io': { channel: 'temp-mail-io', name: 'Temp-Mail.io', website: 'temp-mail.io' },
  'mail-cx': { channel: 'mail-cx', name: 'Mail.cx', website: 'mail.cx' },
  'catchmail': { channel: 'catchmail', name: 'Catchmail', website: 'catchmail.io' },
  'catchmail-mailistry': { channel: 'catchmail-mailistry', name: 'Catchmail Mailistry', website: 'mailistry.com' },
  'catchmail-zeppost': { channel: 'catchmail-zeppost', name: 'Catchmail Zeppost', website: 'zeppost.com' },
  'mailforspam': { channel: 'mailforspam', name: 'MailForSpam', website: 'mailforspam.com' },
  'mailforspam-tempmail-io': { channel: 'mailforspam-tempmail-io', name: 'MailForSpam TempMail.io', website: 'tempmail.io' },
  'mailforspam-disposable': { channel: 'mailforspam-disposable', name: 'MailForSpam Disposable', website: 'disposable.email' },
  'tempmailo': { channel: 'tempmailo', name: 'Tempmailo', website: 'tempmailo.com' },
  'tempmailc': { channel: 'tempmailc', name: 'TempMailC', website: 'tempmailc.com' },
  'mailnesia': { channel: 'mailnesia', name: 'Mailnesia', website: 'mailnesia.com' },
  'throwawaymail': { channel: 'throwawaymail', name: 'ThrowawayMail', website: 'throwawaymail.app' },
  'inboxkitten': { channel: 'inboxkitten', name: 'InboxKitten', website: 'inboxkitten.com' },
  'getnada': { channel: 'getnada', name: 'GetNada', website: 'getnada.net' },
  'mail123': { channel: 'mail123', name: 'Mail123', website: 'mail123.fr' },
  '1sec-mail': { channel: '1sec-mail', name: '1SecMail', website: '1sec-mail.com' },
  'fakemail': { channel: 'fakemail', name: 'FakeMail', website: 'fakemail.net' },
  'openinbox': { channel: 'openinbox', name: 'OpenInbox', website: 'openinbox.io' },
  'inboxes': { channel: 'inboxes', name: 'Inboxes', website: 'inboxes.com' },
  'uncorreotemporal': { channel: 'uncorreotemporal', name: 'UnCorreoTemporal', website: 'uncorreotemporal.com' },
  'awamail': { channel: 'awamail', name: 'AwaMail', website: 'awamail.com' },
  'mail-tm': { channel: 'mail-tm', name: 'Mail.tm', website: 'mail.tm' },
  'dropmail': { channel: 'dropmail', name: 'DropMail', website: 'dropmail.me' },
  'guerrillamail': { channel: 'guerrillamail', name: 'Guerrilla Mail', website: 'guerrillamail.com' },
  'guerrillamail-com': { channel: 'guerrillamail-com', name: 'GuerrillaMail Root', website: 'guerrillamail.com' },
  'maildrop': { channel: 'maildrop', name: 'Maildrop', website: 'maildrop.cx' },
  'smail-pw': { channel: 'smail-pw', name: 'Smail.pw', website: 'smail.pw' },
  'vip-215': { channel: 'vip-215', name: 'VIP 215', website: 'vip.215.im' },
  'fake-legal': { channel: 'fake-legal', name: 'Fake Legal', website: 'fake.legal' },
  'moakt': { channel: 'moakt', name: 'Moakt', website: 'moakt.com' },
  'email10min': { channel: 'email10min', name: 'Email10Min', website: 'email10min.com' },
  'mjj-cm': { channel: 'mjj-cm', name: 'MJJ Mail', website: 'mjj.cm' },
  'linshi-co': { channel: 'linshi-co', name: '临时Co', website: 'linshi.co' },
  'harakirimail': { channel: 'harakirimail', name: 'HarakiriMail', website: 'harakirimail.com' },
  'tempmail-plus': { channel: 'tempmail-plus', name: 'TempMail Plus', website: 'tempmail.plus' },
  'mail-gw': { channel: 'mail-gw', name: 'Mail.gw', website: 'mail.gw' },
  'tempmail-lol-v2': { channel: 'tempmail-lol-v2', name: 'TempMail LOL V2', website: 'tempmail.lol' },
  'sharklasers': { channel: 'sharklasers', name: 'SharkLasers', website: 'sharklasers.com' },
  'sharklasers-com': { channel: 'sharklasers-com', name: 'SharkLasers Root', website: 'sharklasers.com' },
  'grr-la': { channel: 'grr-la', name: 'Grr.la', website: 'grr.la' },
  'grr-la-com': { channel: 'grr-la-com', name: 'Grr.la Root', website: 'grr.la' },
  'guerrillamail-info': { channel: 'guerrillamail-info', name: 'GuerrillaMail Info', website: 'guerrillamail.info' },
  'spam4me': { channel: 'spam4me', name: 'Spam4.me', website: 'spam4.me' },
  'guerrillamail-net': { channel: 'guerrillamail-net', name: 'GuerrillaMail Net', website: 'guerrillamail.net' },
  'guerrillamail-org': { channel: 'guerrillamail-org', name: 'GuerrillaMail Org', website: 'guerrillamail.org' },
  'guerrillamailblock': { channel: 'guerrillamailblock', name: 'GuerrillaMailBlock', website: 'guerrillamailblock.com' },
  'guerrillamail-com-www': { channel: 'guerrillamail-com-www', name: 'GuerrillaMail WWW', website: 'guerrillamail.com' },
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
    case 'temp-mail-io':
      return tempMailIo.generateEmail();
    case 'mail-cx':
      return mailCx.generateEmail(options.domain ?? null);
    case 'catchmail':
      return catchmail.generateEmail(options.domain ?? null);
    case 'catchmail-mailistry':
      return catchmail.generateEmail('mailistry.com', 'catchmail-mailistry');
    case 'catchmail-zeppost':
      return catchmail.generateEmail('zeppost.com', 'catchmail-zeppost');
    case 'mailforspam':
      return mailforspam.generateEmail(options.domain ?? null);
    case 'mailforspam-tempmail-io':
      return mailforspam.generateEmail('tempmail.io', 'mailforspam-tempmail-io');
    case 'mailforspam-disposable':
      return mailforspam.generateEmail('disposable.email', 'mailforspam-disposable');
    case 'tempmailo':
      return tempmailo.generateEmail();
    case 'tempmailc':
      return tempmailc.generateEmail();
    case 'mailnesia':
      return mailnesia.generateEmail();
    case 'throwawaymail':
      return throwawaymail.generateEmail();
    case 'inboxkitten':
      return inboxkitten.generateEmail();
    case 'getnada':
      return getnada.generateEmail();
    case 'mail123':
      return mail123.generateEmail();
    case '1sec-mail':
      return oneSecMail.generateEmail();
    case 'fakemail':
      return fakemail.generateEmail();
    case 'openinbox':
      return openinbox.generateEmail();
    case 'inboxes':
      return inboxes.generateEmail(options.domain ?? null);
    case 'uncorreotemporal':
      return uncorreotemporal.generateEmail();
    case 'awamail':
      return awamail.generateEmail();
    case 'mail-tm':
      return mailTm.generateEmail();
    case 'dropmail':
      return dropmail.generateEmail();
    case 'guerrillamail':
      return guerrillamail.generateEmail();
    case 'guerrillamail-com':
      return guerrillamailComMirror.generateEmail();
    case 'maildrop':
      return maildropProvider.generateEmail(options.domain ?? null);
    case 'smail-pw':
      return smailPw.generateEmail();
    case 'mffac':
      return mffac.generateEmail();
    case 'vip-215':
      return vip215.generateEmail();
    case 'fake-legal':
      return fakeLegal.generateEmail(options.domain ?? null);
    case 'moakt':
      return moakt.generateEmail(options.domain ?? null);
    case 'email10min':
      return email10min.generateEmail();
    case 'mjj-cm':
      return mjjCm.generateEmail();
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
    case 'sharklasers-com':
      return sharklasersCom.generateEmail();
    case 'grr-la':
      return grrla.generateEmail();
    case 'grr-la-com':
      return grrlaCom.generateEmail();
    case 'guerrillamail-info':
      return guerrillainfoMirror.generateEmail();
    case 'spam4me':
      return spam4meMirror.generateEmail();
    case 'guerrillamail-net':
      return guerrillamailNetMirror.generateEmail();
    case 'guerrillamail-org':
      return guerrillamailOrgMirror.generateEmail();
    case 'guerrillamailblock':
      return guerrillamailBlockMirror.generateEmail();
    case 'guerrillamail-com-www':
      return guerrillamailComWwwMirror.generateEmail();
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
    case 'temp-mail-io':
      return tempMailIo.getEmails(email);
    case 'mail-cx':
      if (!token) throw new Error('internal error: token missing for mail-cx');
      return mailCx.getEmails(token, email);
    case 'catchmail':
    case 'catchmail-mailistry':
    case 'catchmail-zeppost':
      return catchmail.getEmails(email);
    case 'mailforspam':
    case 'mailforspam-tempmail-io':
    case 'mailforspam-disposable':
      return mailforspam.getEmails(email);
    case 'tempmailo':
      if (!token) throw new Error('internal error: token missing for tempmailo');
      return tempmailo.getEmails(token, email);
    case 'tempmailc':
      return tempmailc.getEmails(email);
    case 'mailnesia':
      return mailnesia.getEmails(email);
    case 'throwawaymail':
      if (!token) throw new Error('internal error: token missing for throwawaymail');
      return throwawaymail.getEmails(token, email);
    case 'inboxkitten':
      return inboxkitten.getEmails(email);
    case 'getnada':
      if (!token) throw new Error('internal error: token missing for getnada');
      return getnada.getEmails(token, email);
    case 'mail123':
      return mail123.getEmails(email);
    case '1sec-mail':
      if (!token) throw new Error('internal error: token missing for 1sec-mail');
      return oneSecMail.getEmails(token, email);
    case 'fakemail':
      if (!token) throw new Error('internal error: token missing for fakemail');
      return fakemail.getEmails(token, email);
    case 'openinbox':
      if (!token) throw new Error('internal error: token missing for openinbox');
      return openinbox.getEmails(token, email);
    case 'inboxes':
      return inboxes.getEmails(email);
    case 'uncorreotemporal':
      if (!token) throw new Error('internal error: token missing for uncorreotemporal');
      return uncorreotemporal.getEmails(token, email);
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
    case 'guerrillamail-com':
      if (!token) throw new Error('internal error: token missing for guerrillamail-com');
      return guerrillamailComMirror.getEmails(token, email);
    case 'maildrop':
      if (!token) throw new Error('internal error: token missing for maildrop');
      return maildropProvider.getEmails(token, email);
    case 'smail-pw':
      if (!token) throw new Error('internal error: token missing for smail-pw');
      return smailPw.getEmails(token, email);
    case 'mffac':
      return mffac.getEmails(email, token);
    case 'vip-215':
      if (!token) throw new Error('internal error: token missing for vip-215');
      return vip215.getEmails(token, email);
    case 'fake-legal':
      return fakeLegal.getEmails(email);
    case 'moakt':
      if (!token) throw new Error('internal error: token missing for moakt');
      return moakt.getEmails(email, token);
    case 'email10min':
      if (!token) throw new Error('internal error: token missing for email10min');
      return email10min.getEmails(email, token);
    case 'mjj-cm':
      return mjjCm.getEmails(email);
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
    case 'sharklasers-com':
      if (!token) throw new Error('internal error: token missing for sharklasers-com');
      return sharklasersCom.getEmails(token, email);
    case 'grr-la':
      if (!token) throw new Error('internal error: token missing for grr-la');
      return grrla.getEmails(token, email);
    case 'grr-la-com':
      if (!token) throw new Error('internal error: token missing for grr-la-com');
      return grrlaCom.getEmails(token, email);
    case 'guerrillamail-info':
      if (!token) throw new Error('internal error: token missing for guerrillamail-info');
      return guerrillainfoMirror.getEmails(token, email);
    case 'spam4me':
      if (!token) throw new Error('internal error: token missing for spam4me');
      return spam4meMirror.getEmails(token, email);
    case 'guerrillamail-net':
      if (!token) throw new Error('internal error: token missing for guerrillamail-net');
      return guerrillamailNetMirror.getEmails(token, email);
    case 'guerrillamail-org':
      if (!token) throw new Error('internal error: token missing for guerrillamail-org');
      return guerrillamailOrgMirror.getEmails(token, email);
    case 'guerrillamailblock':
      if (!token) throw new Error('internal error: token missing for guerrillamailblock');
      return guerrillamailBlockMirror.getEmails(token, email);
    case 'guerrillamail-com-www':
      if (!token) throw new Error('internal error: token missing for guerrillamail-com-www');
      return guerrillamailComWwwMirror.getEmails(token, email);
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
