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
import * as tempmailc from './providers/tempmailc';
import * as mailnesia from './providers/mailnesia';
import * as throwawaymail from './providers/throwawaymail';
import * as shittyEmail from './providers/shitty-email';
import * as tempmailpro from './providers/tempmailpro';
import * as m2u from './providers/m2u';
import * as tempyEmail from './providers/tempy-email';
import * as devmailUk from './providers/devmail-uk';
import * as cleantempmail from './providers/cleantempmail';
import * as inboxkitten from './providers/inboxkitten';
import * as getnada from './providers/getnada';
import * as mail123 from './providers/mail123';
import * as mail10s from './providers/mail10s';
import * as webmailtemp from './providers/webmailtemp';
import * as tempfastmail from './providers/tempfastmail';
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
import * as zhujump from './providers/zhujump';
import * as tempmailPlus from './providers/tempmail-plus';
import * as tempmailLolV2 from './providers/tempmail-lol-v2';
import * as tempgbox from './providers/tempgbox';
import * as emailnator from './providers/emailnator';
import * as temporam from './providers/temporam';
import * as neighbours from './providers/neighbours';
import * as fmail from './providers/fmail';
import * as ockito from './providers/ockito';
import * as anonbox from './providers/anonbox';
import * as duckmail from './providers/duckmail';
import * as mailinator from './providers/mailinator';
import * as tempmail365 from './providers/tempmail365';
import * as tempinbox from './providers/tempinbox';
import * as byom from './providers/byom';
import * as anonymmail from './providers/anonymmail';
import * as eyepaste from './providers/eyepaste';
import * as segamail from './providers/segamail';
import * as mailSunls from './providers/mail-sunls';
import * as expressinboxhub from './providers/expressinboxhub';
import * as lroid from './providers/lroid';
import * as haribu from './providers/haribu';
import * as pleasenospam from './providers/pleasenospam';
import * as rootsh from './providers/rootsh';
import * as fakeEmailSite from './providers/fake-email-site';
import * as mohmal from './providers/mohmal';
import * as mailgolem from './providers/mailgolem';
import * as bestTempMail from './providers/best-temp-mail';
import * as disposablemailApp from './providers/disposablemail-app';
import * as mailtempCc from './providers/mailtemp-cc';
import * as minuteinbox from './providers/minuteinbox';
import * as mailcatch from './providers/mailcatch';
import * as tempemailCo from './providers/tempemail-co';
import * as tempemailsNet from './providers/tempemails-net';
import * as altmails from './providers/altmails';
import * as tempemailInfo from './providers/tempemail-info';
import * as smailpro from './providers/smailpro';
import * as tempmailten from './providers/tempmailten';
import * as maildropCc from './providers/maildrop-cc';
import * as tenminutemailNet from './providers/10minutemail-net';
import * as linshiyouxiangNet from './providers/linshiyouxiang-net';
import * as tempmailFyi from './providers/tempmail-fyi';
import * as disposablemailCom from './providers/disposablemail-com';
import * as temppMails from './providers/tempp-mails';
import * as emailtempOrg from './providers/emailtemp-org';
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

/** 所有支持的公共渠道列表；随机尝试顺序会基于该列表在本端独立洗牌，不要求跨 SDK 一致 */
const allChannels: Channel[] = ['tempmail', 'tempmail-cn', 'ta-easy', '10minute-one', 'xghff-com', 'oqqaj-com', 'psovv-com', 'dbwot-com', 'ygwpr-com', 'imxwe-com', 'linshiyou', 'mffac', 'tempmail-lol', 'chatgpt-org-uk', 'temp-mail-io', 'mail-cx', 'ddker-com', 'catchmail', 'catchmail-mailistry', 'catchmail-zeppost', 'mailforspam', 'mailforspam-tempmail-io', 'mailforspam-disposable', 'tempmailc', 'mailnesia', 'throwawaymail', 'shitty-email', 'tempmailpro', 'devmail-uk', 'inboxkitten', 'cleantempmail', 'getnada', '1vpn-net', 'abematv-com', 'abematv-net', 'abematv-org', 'aceh-cc', 'bangkabelitung-net', 'cctruyen-com', 'getnada-com', 'getnada-email', 'getnada-net', 'jawatengah-net', 'jawatimur-net', 'kalimantanbarat-net', 'kalimantanselatan-net', 'kalimantantengah-net', 'kalimantantimur-net', 'kalimantanutara-net', 'kepulauanriau-net', 'luxury345-com', 'malukuutara-net', 'nusatenggarabarat-net', 'nusatenggaratimur-net', 'papuabarat-net', 'papuabaratdaya-net', 'papuaselatan-net', 'pehol-com', 'ptruyen-com', 'pulaubali-net', 'riau-net', 'seokey-org', 'sulawesibarat-net', 'sulawesiselatan-net', 'sulawesitengah-net', 'sulawesitenggara-net', 'sumaterabarat-net', 'sumateraselatan-net', 'sumaterautara-net', 'villatogel-com', 'mail123', 'mail10s', 'webmailtemp', 'tempfastmail', '1sec-mail', 'fakemail', 'openinbox', 'inboxes', 'uncorreotemporal', 'awamail', 'mail-tm', 'web-library-net', 'dropmail', 'guerrillamail', 'guerrillamail-com', 'maildrop', 'smail-pw', 'vip-215', 'fake-legal', 'imgui-de', 'pulsewebmenu-de', 'moakt', 'drmail-in', 'teml-net', 'tmpeml-com', 'tmpbox-net', 'moakt-cc', 'disbox-net', 'tmpmail-org', 'tmpmail-net', 'tmails-net', 'disbox-org', 'moakt-co', 'moakt-ws', 'tmail-ws', 'bareed-ws', 'email10min', 'mjj-cm', 'linshi-co', 'harakirimail', 'jqkjqk-xyz', 'lyhlevi-com', 'tempmail-plus', 'fexpost-com', 'fexbox-org', 'mailbox-in-ua', 'rover-info', 'chitthi-in', 'fextemp-com', 'any-pink', 'merepost-com', 'tempmail-lol-v2', 'tempgbox', 'emailnator', 'temporam', 'neighbours', 'sharklasers', 'sharklasers-com', 'grr-la', 'grr-la-com', 'guerrillamail-info', 'spam4me', 'guerrillamail-net', 'guerrillamail-org', 'guerrillamailblock', 'guerrillamail-com-www', 'm2u', 'tempy-email', 'fmail', 'ockito', 'anonbox', 'duckmail', 'mailinator', 'tempmail365', 'tempinbox', 'byom', 'anonymmail', 'eyepaste', 'segamail', 'mail-sunls', 'expressinboxhub', 'lroid', 'haribu', 'pleasenospam', 'rootsh', 'fake-email-site', 'mohmal', 'mailgolem', 'best-temp-mail', 'disposablemail-app', 'mailtemp-cc', 'minuteinbox', 'mailcatch', 'tempemail-co', 'tempemails-net', 'altmails', 'tempemail-info', 'smailpro', 'tempmailten', 'maildrop-cc', '10minutemail-net', 'linshiyouxiang-net', 'tempmail-fyi', 'disposablemail-com', 'tempp-mails', 'emailtemp-org'];

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
  'xghff-com': { channel: 'xghff-com', name: 'xghff.com', website: '10minutemail.one' },
  'oqqaj-com': { channel: 'oqqaj-com', name: 'oqqaj.com', website: '10minutemail.one' },
  'psovv-com': { channel: 'psovv-com', name: 'psovv.com', website: '10minutemail.one' },
  'dbwot-com': { channel: 'dbwot-com', name: 'dbwot.com', website: '10minutemail.one' },
  'ygwpr-com': { channel: 'ygwpr-com', name: 'ygwpr.com', website: '10minutemail.one' },
  'imxwe-com': { channel: 'imxwe-com', name: 'imxwe.com', website: '10minutemail.one' },
  'linshiyou': { channel: 'linshiyou', name: '临时邮', website: 'linshiyou.com' },
  'mffac': { channel: 'mffac', name: 'MFFAC', website: 'mffac.com' },
  'tempmail-lol': { channel: 'tempmail-lol', name: 'TempMail LOL', website: 'tempmail.lol' },
  'chatgpt-org-uk': { channel: 'chatgpt-org-uk', name: 'ChatGPT Mail', website: 'mail.chatgpt.org.uk' },
  'temp-mail-io': { channel: 'temp-mail-io', name: 'Temp-Mail.io', website: 'temp-mail.io' },
  'mail-cx': { channel: 'mail-cx', name: 'Mail.cx', website: 'mail.cx' },
  'ddker-com': { channel: 'ddker-com', name: 'ddker.com', website: 'mail.cx' },
  'catchmail': { channel: 'catchmail', name: 'Catchmail', website: 'catchmail.io' },
  'catchmail-mailistry': { channel: 'catchmail-mailistry', name: 'Catchmail Mailistry', website: 'mailistry.com' },
  'catchmail-zeppost': { channel: 'catchmail-zeppost', name: 'Catchmail Zeppost', website: 'zeppost.com' },
  'mailforspam': { channel: 'mailforspam', name: 'MailForSpam', website: 'mailforspam.com' },
  'mailforspam-tempmail-io': { channel: 'mailforspam-tempmail-io', name: 'MailForSpam TempMail.io', website: 'tempmail.io' },
  'mailforspam-disposable': { channel: 'mailforspam-disposable', name: 'MailForSpam Disposable', website: 'disposable.email' },
  'tempmailc': { channel: 'tempmailc', name: 'TempMailC', website: 'tempmailc.com' },
  'mailnesia': { channel: 'mailnesia', name: 'Mailnesia', website: 'mailnesia.com' },
  'throwawaymail': { channel: 'throwawaymail', name: 'ThrowawayMail', website: 'throwawaymail.app' },
  'shitty-email': { channel: 'shitty-email', name: 'shitty.email', website: 'shitty.email' },
  'tempmailpro': { channel: 'tempmailpro', name: 'TempMail Pro', website: 'tempmailpro.us' },
  'devmail-uk': { channel: 'devmail-uk', name: 'DevMail UK', website: 'devmail.uk' },
  'cleantempmail': { channel: 'cleantempmail', name: 'CleanTempMail', website: 'cleantempmail.com' },
  'inboxkitten': { channel: 'inboxkitten', name: 'InboxKitten', website: 'inboxkitten.com' },
  'getnada': { channel: 'getnada', name: 'GetNada', website: 'getnada.net' },
  '1vpn-net': { channel: '1vpn-net', name: '1vpn.net', website: 'getnada.net' },
  'abematv-com': { channel: 'abematv-com', name: 'abematv.com', website: 'getnada.net' },
  'abematv-net': { channel: 'abematv-net', name: 'abematv.net', website: 'getnada.net' },
  'abematv-org': { channel: 'abematv-org', name: 'abematv.org', website: 'getnada.net' },
  'aceh-cc': { channel: 'aceh-cc', name: 'aceh.cc', website: 'getnada.net' },
  'bangkabelitung-net': { channel: 'bangkabelitung-net', name: 'bangkabelitung.net', website: 'getnada.net' },
  'cctruyen-com': { channel: 'cctruyen-com', name: 'cctruyen.com', website: 'getnada.net' },
  'getnada-com': { channel: 'getnada-com', name: 'getnada.com', website: 'getnada.net' },
  'getnada-email': { channel: 'getnada-email', name: 'getnada.email', website: 'getnada.net' },
  'getnada-net': { channel: 'getnada-net', name: 'getnada.net', website: 'getnada.net' },
  'jawatengah-net': { channel: 'jawatengah-net', name: 'jawatengah.net', website: 'getnada.net' },
  'jawatimur-net': { channel: 'jawatimur-net', name: 'jawatimur.net', website: 'getnada.net' },
  'kalimantanbarat-net': { channel: 'kalimantanbarat-net', name: 'kalimantanbarat.net', website: 'getnada.net' },
  'kalimantanselatan-net': { channel: 'kalimantanselatan-net', name: 'kalimantanselatan.net', website: 'getnada.net' },
  'kalimantantengah-net': { channel: 'kalimantantengah-net', name: 'kalimantantengah.net', website: 'getnada.net' },
  'kalimantantimur-net': { channel: 'kalimantantimur-net', name: 'kalimantantimur.net', website: 'getnada.net' },
  'kalimantanutara-net': { channel: 'kalimantanutara-net', name: 'kalimantanutara.net', website: 'getnada.net' },
  'kepulauanriau-net': { channel: 'kepulauanriau-net', name: 'kepulauanriau.net', website: 'getnada.net' },
  'luxury345-com': { channel: 'luxury345-com', name: 'luxury345.com', website: 'getnada.net' },
  'malukuutara-net': { channel: 'malukuutara-net', name: 'malukuutara.net', website: 'getnada.net' },
  'nusatenggarabarat-net': { channel: 'nusatenggarabarat-net', name: 'nusatenggarabarat.net', website: 'getnada.net' },
  'nusatenggaratimur-net': { channel: 'nusatenggaratimur-net', name: 'nusatenggaratimur.net', website: 'getnada.net' },
  'papuabarat-net': { channel: 'papuabarat-net', name: 'papuabarat.net', website: 'getnada.net' },
  'papuabaratdaya-net': { channel: 'papuabaratdaya-net', name: 'papuabaratdaya.net', website: 'getnada.net' },
  'papuaselatan-net': { channel: 'papuaselatan-net', name: 'papuaselatan.net', website: 'getnada.net' },
  'pehol-com': { channel: 'pehol-com', name: 'pehol.com', website: 'getnada.net' },
  'ptruyen-com': { channel: 'ptruyen-com', name: 'ptruyen.com', website: 'getnada.net' },
  'pulaubali-net': { channel: 'pulaubali-net', name: 'pulaubali.net', website: 'getnada.net' },
  'riau-net': { channel: 'riau-net', name: 'riau.net', website: 'getnada.net' },
  'seokey-org': { channel: 'seokey-org', name: 'seokey.org', website: 'getnada.net' },
  'sulawesibarat-net': { channel: 'sulawesibarat-net', name: 'sulawesibarat.net', website: 'getnada.net' },
  'sulawesiselatan-net': { channel: 'sulawesiselatan-net', name: 'sulawesiselatan.net', website: 'getnada.net' },
  'sulawesitengah-net': { channel: 'sulawesitengah-net', name: 'sulawesitengah.net', website: 'getnada.net' },
  'sulawesitenggara-net': { channel: 'sulawesitenggara-net', name: 'sulawesitenggara.net', website: 'getnada.net' },
  'sumaterabarat-net': { channel: 'sumaterabarat-net', name: 'sumaterabarat.net', website: 'getnada.net' },
  'sumateraselatan-net': { channel: 'sumateraselatan-net', name: 'sumateraselatan.net', website: 'getnada.net' },
  'sumaterautara-net': { channel: 'sumaterautara-net', name: 'sumaterautara.net', website: 'getnada.net' },
  'villatogel-com': { channel: 'villatogel-com', name: 'villatogel.com', website: 'getnada.net' },
  'mail123': { channel: 'mail123', name: 'Mail123', website: 'mail123.fr' },
  'mail10s': { channel: 'mail10s', name: 'Mail10s', website: 'mail10s.com' },
  'webmailtemp': { channel: 'webmailtemp', name: 'WebMailTemp', website: 'webmailtemp.com' },
  'tempfastmail': { channel: 'tempfastmail', name: 'TempFastMail', website: 'tempfastmail.com' },
  '1sec-mail': { channel: '1sec-mail', name: '1SecMail', website: '1sec-mail.com' },
  'fakemail': { channel: 'fakemail', name: 'FakeMail', website: 'fakemail.net' },
  'openinbox': { channel: 'openinbox', name: 'OpenInbox', website: 'openinbox.io' },
  'inboxes': { channel: 'inboxes', name: 'Inboxes', website: 'inboxes.com' },
  'uncorreotemporal': { channel: 'uncorreotemporal', name: 'UnCorreoTemporal', website: 'uncorreotemporal.com' },
  'awamail': { channel: 'awamail', name: 'AwaMail', website: 'awamail.com' },
  'mail-tm': { channel: 'mail-tm', name: 'Mail.tm', website: 'mail.tm' },
  'web-library-net': { channel: 'web-library-net', name: 'web-library.net', website: 'mail.tm' },
  'dropmail': { channel: 'dropmail', name: 'DropMail', website: 'dropmail.me' },
  'guerrillamail': { channel: 'guerrillamail', name: 'Guerrilla Mail', website: 'guerrillamail.com' },
  'guerrillamail-com': { channel: 'guerrillamail-com', name: 'GuerrillaMail Root', website: 'guerrillamail.com' },
  'maildrop': { channel: 'maildrop', name: 'Maildrop', website: 'maildrop.cx' },
  'smail-pw': { channel: 'smail-pw', name: 'Smail.pw', website: 'smail.pw' },
  'vip-215': { channel: 'vip-215', name: 'VIP 215', website: 'vip.215.im' },
  'fake-legal': { channel: 'fake-legal', name: 'Fake Legal', website: 'fake.legal' },
  'imgui-de': { channel: 'imgui-de', name: 'imgui.de', website: 'fake.legal' },
  'pulsewebmenu-de': { channel: 'pulsewebmenu-de', name: 'pulsewebmenu.de', website: 'fake.legal' },
  'moakt': { channel: 'moakt', name: 'Moakt', website: 'moakt.com' },
  'drmail-in': { channel: 'drmail-in', name: 'drmail.in', website: 'moakt.com' },
  'teml-net': { channel: 'teml-net', name: 'teml.net', website: 'moakt.com' },
  'tmpeml-com': { channel: 'tmpeml-com', name: 'tmpeml.com', website: 'moakt.com' },
  'tmpbox-net': { channel: 'tmpbox-net', name: 'tmpbox.net', website: 'moakt.com' },
  'moakt-cc': { channel: 'moakt-cc', name: 'moakt.cc', website: 'moakt.com' },
  'disbox-net': { channel: 'disbox-net', name: 'disbox.net', website: 'moakt.com' },
  'tmpmail-org': { channel: 'tmpmail-org', name: 'tmpmail.org', website: 'moakt.com' },
  'tmpmail-net': { channel: 'tmpmail-net', name: 'tmpmail.net', website: 'moakt.com' },
  'tmails-net': { channel: 'tmails-net', name: 'tmails.net', website: 'moakt.com' },
  'disbox-org': { channel: 'disbox-org', name: 'disbox.org', website: 'moakt.com' },
  'moakt-co': { channel: 'moakt-co', name: 'moakt.co', website: 'moakt.com' },
  'moakt-ws': { channel: 'moakt-ws', name: 'moakt.ws', website: 'moakt.com' },
  'tmail-ws': { channel: 'tmail-ws', name: 'tmail.ws', website: 'moakt.com' },
  'bareed-ws': { channel: 'bareed-ws', name: 'bareed.ws', website: 'moakt.com' },
  'email10min': { channel: 'email10min', name: 'Email10Min', website: 'email10min.com' },
  'mjj-cm': { channel: 'mjj-cm', name: 'MJJ Mail', website: 'mjj.cm' },
  'linshi-co': { channel: 'linshi-co', name: '临时Co', website: 'linshi.co' },
  'harakirimail': { channel: 'harakirimail', name: 'HarakiriMail', website: 'harakirimail.com' },
  'jqkjqk-xyz': { channel: 'jqkjqk-xyz', name: 'jqkjqk.xyz', website: 'mail.zhujump.com' },
  'lyhlevi-com': { channel: 'lyhlevi-com', name: 'LyhLevi MoeMail', website: 'lyhlevi.com' },
  'tempmail-plus': { channel: 'tempmail-plus', name: 'TempMail Plus', website: 'tempmail.plus' },
  'fexpost-com': { channel: 'fexpost-com', name: 'fexpost.com', website: 'tempmail.plus' },
  'fexbox-org': { channel: 'fexbox-org', name: 'fexbox.org', website: 'tempmail.plus' },
  'mailbox-in-ua': { channel: 'mailbox-in-ua', name: 'mailbox.in.ua', website: 'tempmail.plus' },
  'rover-info': { channel: 'rover-info', name: 'rover.info', website: 'tempmail.plus' },
  'chitthi-in': { channel: 'chitthi-in', name: 'chitthi.in', website: 'tempmail.plus' },
  'fextemp-com': { channel: 'fextemp-com', name: 'fextemp.com', website: 'tempmail.plus' },
  'any-pink': { channel: 'any-pink', name: 'any.pink', website: 'tempmail.plus' },
  'merepost-com': { channel: 'merepost-com', name: 'merepost.com', website: 'tempmail.plus' },
  'tempmail-lol-v2': { channel: 'tempmail-lol-v2', name: 'TempMail LOL V2', website: 'tempmail.lol' },
  'tempgbox': { channel: 'tempgbox', name: 'TempGBox', website: 'tempgbox.net' },
  'emailnator': { channel: 'emailnator', name: 'Emailnator', website: 'emailnator.com' },
  'temporam': { channel: 'temporam', name: 'Temporam', website: 'temporam.com' },
  'neighbours': { channel: 'neighbours', name: 'Neighbours', website: 'neighbours.sh' },
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
  'm2u': { channel: 'm2u', name: 'MailToYou', website: 'm2u.io' },
  'tempy-email': { channel: 'tempy-email', name: 'Tempy Email', website: 'tempy.email' },
  'fmail': { channel: 'fmail', name: 'FMail', website: 'fmail.men' },
  'ockito': { channel: 'ockito', name: 'Ockito', website: 'ockito.com' },
  'anonbox': { channel: 'anonbox', name: 'Anonbox', website: 'anonbox.net' },
  'duckmail': { channel: 'duckmail', name: 'DuckMail', website: 'duckmail.sbs' },
  'mailinator': { channel: 'mailinator', name: 'Mailinator', website: 'mailinator.com' },
  'tempmail365': { channel: 'tempmail365', name: 'Tempmail365', website: 'https://tempmail365.cn' },
  'tempinbox': { channel: 'tempinbox', name: 'TempInbox', website: 'https://www.tempinbox.xyz' },
  'byom': { channel: 'byom', name: 'Byom', website: 'byom.de' },
  'anonymmail': { channel: 'anonymmail', name: 'AnonymMail', website: 'anonymmail.net' },
  'eyepaste': { channel: 'eyepaste', name: 'EyePaste', website: 'eyepaste.com' },
  'segamail': { channel: 'segamail', name: 'SegaMail', website: 'segamail.com' },
  'mail-sunls': { channel: 'mail-sunls', name: 'Mail Sunls', website: 'mail.sunls.de' },
  'expressinboxhub': { channel: 'expressinboxhub', name: 'ExpressInboxHub', website: 'expressinboxhub.com' },
  'lroid': { channel: 'lroid', name: 'Lroid', website: 'lroid.com' },
  'haribu': { channel: 'haribu', name: 'Haribu', website: 'haribu.net' },
  'pleasenospam': { channel: 'pleasenospam', name: 'PleeaseNoSpam', website: 'pleasenospam.email' },
  'rootsh': { channel: 'rootsh', name: 'Rootsh(BccTo)', website: 'rootsh.com' },
  'fake-email-site': { channel: 'fake-email-site', name: 'FakeEmailSite', website: 'fake-email.site' },
  'mohmal': { channel: 'mohmal', name: 'Mohmal', website: 'mohmal.com' },
  'mailgolem': { channel: 'mailgolem', name: 'MailGolem', website: 'mailgolem.com' },
  'best-temp-mail': { channel: 'best-temp-mail', name: 'BestTempMail', website: 'best-temp-mail.com' },
  'disposablemail-app': { channel: 'disposablemail-app', name: 'DisposableMail', website: 'disposablemail.app' },
  'mailtemp-cc': { channel: 'mailtemp-cc', name: 'MailTemp.cc', website: 'mailtemp.cc' },
  'minuteinbox': { channel: 'minuteinbox', name: 'MinuteInbox', website: 'minuteinbox.com' },
  'mailcatch': { channel: 'mailcatch', name: 'MailCatch', website: 'mailcatch.com' },
  'tempemail-co': { channel: 'tempemail-co', name: 'TempEmail.co', website: 'tempemail.co' },
  'tempemails-net': { channel: 'tempemails-net', name: 'TempEmails.net', website: 'tempemails.net' },
  'altmails': { channel: 'altmails', name: 'AltMails', website: 'tempmail.altmails.com' },
  'tempemail-info': { channel: 'tempemail-info', name: 'TempEmailInfo', website: 'tempemail.info' },
  'smailpro': { channel: 'smailpro', name: 'SmailPro', website: 'smailpro.com' },
  'tempmailten': { channel: 'tempmailten', name: 'TempMailTen', website: 'tempmailten.com' },
  'maildrop-cc': { channel: 'maildrop-cc', name: 'MailDrop.cc', website: 'maildrop.cc' },
  '10minutemail-net': { channel: '10minutemail-net', name: '10MinuteMail.net', website: '10minutemail.net' },
  'linshiyouxiang-net': { channel: 'linshiyouxiang-net', name: '临时邮箱(linshiyouxiang)', website: 'linshiyouxiang.net' },
  'tempmail-fyi': { channel: 'tempmail-fyi', name: 'Temp-Mail.fyi', website: 'temp-mail.fyi' },
  'disposablemail-com': { channel: 'disposablemail-com', name: 'DisposableMail.com', website: 'disposablemail.com' },
  'tempp-mails': { channel: 'tempp-mails', name: 'TemppMails', website: 'tempp-mails.com' },
  'emailtemp-org': { channel: 'emailtemp-org', name: 'EmailTemp', website: 'emailtemp.org' },
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
 * Fisher-Yates 洗牌算法，生成均匀分布的随机排列
 */
function shuffleArray<T>(arr: T[]): T[] {
  const result = [...arr];
  for (let i = result.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    [result[i], result[j]] = [result[j], result[i]];
  }
  return result;
}

/**
 * 构建渠道尝试顺序
 * 指定渠道时优先尝试该渠道，其余渠道打乱追加
 * 未指定时打乱全部渠道
 * 本函数返回的是本端运行时随机顺序，不作为跨 SDK 一致性约束
 */
function buildChannelOrder(preferred?: Channel, allowFallback = true): Channel[] {
  const shuffled = shuffleArray(allChannels);
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
    case 'xghff-com':
      return tenminuteOne.generateEmail('xghff.com').then(info => ({ ...info, channel: 'xghff-com' }));
    case 'oqqaj-com':
      return tenminuteOne.generateEmail('oqqaj.com').then(info => ({ ...info, channel: 'oqqaj-com' }));
    case 'psovv-com':
      return tenminuteOne.generateEmail('psovv.com').then(info => ({ ...info, channel: 'psovv-com' }));
    case 'dbwot-com':
      return tenminuteOne.generateEmail('dbwot.com').then(info => ({ ...info, channel: 'dbwot-com' }));
    case 'ygwpr-com':
      return tenminuteOne.generateEmail('ygwpr.com').then(info => ({ ...info, channel: 'ygwpr-com' }));
    case 'imxwe-com':
      return tenminuteOne.generateEmail('imxwe.com').then(info => ({ ...info, channel: 'imxwe-com' }));
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
    case 'ddker-com':
      return mailCx.generateEmail('ddker.com').then(info => ({ ...info, channel: 'ddker-com' }));
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
    case 'tempmailc':
      return tempmailc.generateEmail();
    case 'mailnesia':
      return mailnesia.generateEmail();
    case 'throwawaymail':
      return throwawaymail.generateEmail();
    case 'shitty-email':
      return shittyEmail.generateEmail();
    case 'tempmailpro':
      return tempmailpro.generateEmail();
    case 'devmail-uk':
      return devmailUk.generateEmail();
    case 'cleantempmail':
      return cleantempmail.generateEmail();
    case 'm2u':
      return m2u.generateEmail();
    case 'tempy-email':
      return tempyEmail.generateEmail(options.domain ?? null);
    case 'inboxkitten':
      return inboxkitten.generateEmail();
    case 'getnada':
      return getnada.generateEmail(options.domain ?? null);
    case '1vpn-net':
      return getnada.generateEmail('1vpn.net', '1vpn-net');
    case 'abematv-com':
      return getnada.generateEmail('abematv.com', 'abematv-com');
    case 'abematv-net':
      return getnada.generateEmail('abematv.net', 'abematv-net');
    case 'abematv-org':
      return getnada.generateEmail('abematv.org', 'abematv-org');
    case 'aceh-cc':
      return getnada.generateEmail('aceh.cc', 'aceh-cc');
    case 'bangkabelitung-net':
      return getnada.generateEmail('bangkabelitung.net', 'bangkabelitung-net');
    case 'cctruyen-com':
      return getnada.generateEmail('cctruyen.com', 'cctruyen-com');
    case 'getnada-com':
      return getnada.generateEmail('getnada.com', 'getnada-com');
    case 'getnada-email':
      return getnada.generateEmail('getnada.email', 'getnada-email');
    case 'getnada-net':
      return getnada.generateEmail('getnada.net', 'getnada-net');
    case 'jawatengah-net':
      return getnada.generateEmail('jawatengah.net', 'jawatengah-net');
    case 'jawatimur-net':
      return getnada.generateEmail('jawatimur.net', 'jawatimur-net');
    case 'kalimantanbarat-net':
      return getnada.generateEmail('kalimantanbarat.net', 'kalimantanbarat-net');
    case 'kalimantanselatan-net':
      return getnada.generateEmail('kalimantanselatan.net', 'kalimantanselatan-net');
    case 'kalimantantengah-net':
      return getnada.generateEmail('kalimantantengah.net', 'kalimantantengah-net');
    case 'kalimantantimur-net':
      return getnada.generateEmail('kalimantantimur.net', 'kalimantantimur-net');
    case 'kalimantanutara-net':
      return getnada.generateEmail('kalimantanutara.net', 'kalimantanutara-net');
    case 'kepulauanriau-net':
      return getnada.generateEmail('kepulauanriau.net', 'kepulauanriau-net');
    case 'luxury345-com':
      return getnada.generateEmail('luxury345.com', 'luxury345-com');
    case 'malukuutara-net':
      return getnada.generateEmail('malukuutara.net', 'malukuutara-net');
    case 'nusatenggarabarat-net':
      return getnada.generateEmail('nusatenggarabarat.net', 'nusatenggarabarat-net');
    case 'nusatenggaratimur-net':
      return getnada.generateEmail('nusatenggaratimur.net', 'nusatenggaratimur-net');
    case 'papuabarat-net':
      return getnada.generateEmail('papuabarat.net', 'papuabarat-net');
    case 'papuabaratdaya-net':
      return getnada.generateEmail('papuabaratdaya.net', 'papuabaratdaya-net');
    case 'papuaselatan-net':
      return getnada.generateEmail('papuaselatan.net', 'papuaselatan-net');
    case 'pehol-com':
      return getnada.generateEmail('pehol.com', 'pehol-com');
    case 'ptruyen-com':
      return getnada.generateEmail('ptruyen.com', 'ptruyen-com');
    case 'pulaubali-net':
      return getnada.generateEmail('pulaubali.net', 'pulaubali-net');
    case 'riau-net':
      return getnada.generateEmail('riau.net', 'riau-net');
    case 'seokey-org':
      return getnada.generateEmail('seokey.org', 'seokey-org');
    case 'sulawesibarat-net':
      return getnada.generateEmail('sulawesibarat.net', 'sulawesibarat-net');
    case 'sulawesiselatan-net':
      return getnada.generateEmail('sulawesiselatan.net', 'sulawesiselatan-net');
    case 'sulawesitengah-net':
      return getnada.generateEmail('sulawesitengah.net', 'sulawesitengah-net');
    case 'sulawesitenggara-net':
      return getnada.generateEmail('sulawesitenggara.net', 'sulawesitenggara-net');
    case 'sumaterabarat-net':
      return getnada.generateEmail('sumaterabarat.net', 'sumaterabarat-net');
    case 'sumateraselatan-net':
      return getnada.generateEmail('sumateraselatan.net', 'sumateraselatan-net');
    case 'sumaterautara-net':
      return getnada.generateEmail('sumaterautara.net', 'sumaterautara-net');
    case 'villatogel-com':
      return getnada.generateEmail('villatogel.com', 'villatogel-com');
    case 'mail123':
      return mail123.generateEmail();
    case 'mail10s':
      return mail10s.generateEmail();
    case 'webmailtemp':
      return webmailtemp.generateEmail();
    case 'tempfastmail':
      return tempfastmail.generateEmail();
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
    case 'web-library-net':
      return mailTm.generateEmail().then(info => ({ ...info, channel: 'web-library-net' }));
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
    case 'imgui-de':
      return fakeLegal.generateEmail('imgui.de', 'imgui-de');
    case 'pulsewebmenu-de':
      return fakeLegal.generateEmail('pulsewebmenu.de', 'pulsewebmenu-de');
    case 'moakt':
      return moakt.generateEmail(options.domain ?? null);
    case 'drmail-in':
      return moakt.generateEmail('drmail.in').then(info => ({ ...info, channel: 'drmail-in' }));
    case 'teml-net':
      return moakt.generateEmail('teml.net').then(info => ({ ...info, channel: 'teml-net' }));
    case 'tmpeml-com':
      return moakt.generateEmail('tmpeml.com').then(info => ({ ...info, channel: 'tmpeml-com' }));
    case 'tmpbox-net':
      return moakt.generateEmail('tmpbox.net').then(info => ({ ...info, channel: 'tmpbox-net' }));
    case 'moakt-cc':
      return moakt.generateEmail('moakt.cc').then(info => ({ ...info, channel: 'moakt-cc' }));
    case 'disbox-net':
      return moakt.generateEmail('disbox.net').then(info => ({ ...info, channel: 'disbox-net' }));
    case 'tmpmail-org':
      return moakt.generateEmail('tmpmail.org').then(info => ({ ...info, channel: 'tmpmail-org' }));
    case 'tmpmail-net':
      return moakt.generateEmail('tmpmail.net').then(info => ({ ...info, channel: 'tmpmail-net' }));
    case 'tmails-net':
      return moakt.generateEmail('tmails.net').then(info => ({ ...info, channel: 'tmails-net' }));
    case 'disbox-org':
      return moakt.generateEmail('disbox.org').then(info => ({ ...info, channel: 'disbox-org' }));
    case 'moakt-co':
      return moakt.generateEmail('moakt.co').then(info => ({ ...info, channel: 'moakt-co' }));
    case 'moakt-ws':
      return moakt.generateEmail('moakt.ws').then(info => ({ ...info, channel: 'moakt-ws' }));
    case 'tmail-ws':
      return moakt.generateEmail('tmail.ws').then(info => ({ ...info, channel: 'tmail-ws' }));
    case 'bareed-ws':
      return moakt.generateEmail('bareed.ws').then(info => ({ ...info, channel: 'bareed-ws' }));
    case 'email10min':
      return email10min.generateEmail();
    case 'mjj-cm':
      return mjjCm.generateEmail();
    case 'linshi-co':
      return linshiCo.generateEmail();
    case 'harakirimail':
      return harakirimail.generateEmail();
    case 'jqkjqk-xyz':
      return zhujump.generateEmail('jqkjqk.xyz', 'jqkjqk-xyz');
    case 'lyhlevi-com':
      return zhujump.generateEmailForInstance({
        baseUrl: 'https://lyhlevi.com',
        domain: 'lyhlevi.com',
        channel: 'lyhlevi-com',
        expiryTime: 24 * 60 * 60 * 1000,
      });
    case 'tempmail-plus':
      return tempmailPlus.generateEmail();
    case 'fexpost-com':
      return tempmailPlus.generateEmail('fexpost.com', 'fexpost-com');
    case 'fexbox-org':
      return tempmailPlus.generateEmail('fexbox.org', 'fexbox-org');
    case 'mailbox-in-ua':
      return tempmailPlus.generateEmail('mailbox.in.ua', 'mailbox-in-ua');
    case 'rover-info':
      return tempmailPlus.generateEmail('rover.info', 'rover-info');
    case 'chitthi-in':
      return tempmailPlus.generateEmail('chitthi.in', 'chitthi-in');
    case 'fextemp-com':
      return tempmailPlus.generateEmail('fextemp.com', 'fextemp-com');
    case 'any-pink':
      return tempmailPlus.generateEmail('any.pink', 'any-pink');
    case 'merepost-com':
      return tempmailPlus.generateEmail('merepost.com', 'merepost-com');
    case 'tempmail-lol-v2':
      return tempmailLolV2.generateEmail();
    case 'tempgbox':
      return tempgbox.generateEmail();
    case 'emailnator':
      return emailnator.generateEmail();
    case 'temporam':
      return temporam.generateEmail(options.domain ?? null);
    case 'neighbours':
      return neighbours.generateEmail(options.domain ?? null);
    case 'mailinator':
      return mailinator.generateEmail();
    case 'tempmail365':
      return tempmail365.generateEmail(options.domain ?? null);
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
    case 'm2u':
      return m2u.generateEmail();
    case 'tempy-email':
      return tempyEmail.generateEmail(options.domain ?? null);
    case 'tempinbox':
      return tempinbox.generateEmail(options.domain ?? null);
    case 'byom':
      return byom.generateEmail();
    case 'anonymmail':
      return anonymmail.generateEmail();
    case 'eyepaste':
      return eyepaste.generateEmail();
    case 'segamail':
      return segamail.generateEmail();
    case 'mail-sunls':
      return mailSunls.generateEmail();
    case 'expressinboxhub':
      return expressinboxhub.generateEmail();
    case 'lroid':
      return lroid.generateEmail();
    case 'haribu':
      return haribu.generateEmail();
    case 'pleasenospam':
      return pleasenospam.generateEmail(options.domain ?? null);
    case 'rootsh':
      return rootsh.generateEmail(options.domain ?? undefined);
    case 'fake-email-site':
      return fakeEmailSite.generateEmail();
    case 'mohmal':
      return mohmal.generateEmail();
    case 'mailgolem':
      return mailgolem.generateEmail();
    case 'best-temp-mail':
      return bestTempMail.generateEmail();
    case 'disposablemail-app':
      return disposablemailApp.generateEmail();
    case 'mailtemp-cc':
      return mailtempCc.generateEmail();
    case 'minuteinbox':
      return minuteinbox.generateEmail();
    case 'mailcatch':
      return mailcatch.generateEmail();
    case 'tempemail-co':
      return tempemailCo.generateEmail();
    case 'tempemails-net':
      return tempemailsNet.generateEmail().then(info => ({ ...info, channel }));
    case 'altmails':
      return altmails.generateEmail();
    case 'tempemail-info':
      return tempemailInfo.generateEmail();
    case 'smailpro':
      return smailpro.generateEmail();
    case 'tempmailten':
      return tempmailten.generateEmail();
    case 'maildrop-cc':
      return maildropCc.generateEmail();
    case '10minutemail-net':
      return tenminutemailNet.generateEmail();
    case 'linshiyouxiang-net':
      return linshiyouxiangNet.generateEmail();
    case 'tempmail-fyi':
      return tempmailFyi.generateEmail();
    case 'disposablemail-com':
      return disposablemailCom.generateEmail();
    case 'tempp-mails':
      return temppMails.generateEmail();
    case 'emailtemp-org':
      return emailtempOrg.generateEmail();
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
    case 'xghff-com':
    case 'oqqaj-com':
    case 'psovv-com':
    case 'dbwot-com':
    case 'ygwpr-com':
    case 'imxwe-com':
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
    case 'ddker-com':
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
    case 'tempmailc':
      return tempmailc.getEmails(email);
    case 'mailnesia':
      return mailnesia.getEmails(email);
    case 'throwawaymail':
      if (!token) throw new Error('internal error: token missing for throwawaymail');
      return throwawaymail.getEmails(token, email);
    case 'shitty-email':
      if (!token) throw new Error('internal error: token missing for shitty-email');
      return shittyEmail.getEmails(token, email);
    case 'tempmailpro':
      if (!token) throw new Error('internal error: token missing for tempmailpro');
      return tempmailpro.getEmails(token, email);
    case 'devmail-uk':
      return devmailUk.getEmails(email);
    case 'cleantempmail':
      return cleantempmail.getEmails(email);
    case 'inboxkitten':
      return inboxkitten.getEmails(email);
    case 'getnada':
    case '1vpn-net':
    case 'abematv-com':
    case 'abematv-net':
    case 'abematv-org':
    case 'aceh-cc':
    case 'bangkabelitung-net':
    case 'cctruyen-com':
    case 'getnada-com':
    case 'getnada-email':
    case 'getnada-net':
    case 'jawatengah-net':
    case 'jawatimur-net':
    case 'kalimantanbarat-net':
    case 'kalimantanselatan-net':
    case 'kalimantantengah-net':
    case 'kalimantantimur-net':
    case 'kalimantanutara-net':
    case 'kepulauanriau-net':
    case 'luxury345-com':
    case 'malukuutara-net':
    case 'nusatenggarabarat-net':
    case 'nusatenggaratimur-net':
    case 'papuabarat-net':
    case 'papuabaratdaya-net':
    case 'papuaselatan-net':
    case 'pehol-com':
    case 'ptruyen-com':
    case 'pulaubali-net':
    case 'riau-net':
    case 'seokey-org':
    case 'sulawesibarat-net':
    case 'sulawesiselatan-net':
    case 'sulawesitengah-net':
    case 'sulawesitenggara-net':
    case 'sumaterabarat-net':
    case 'sumateraselatan-net':
    case 'sumaterautara-net':
    case 'villatogel-com':
      if (!token) throw new Error('internal error: token missing for getnada');
      return getnada.getEmails(token, email);
    case 'mail123':
      return mail123.getEmails(email);
    case 'mail10s':
      return mail10s.getEmails(email);
    case 'webmailtemp':
      if (!token) throw new Error('internal error: token missing for webmailtemp');
      return webmailtemp.getEmails(token, email);
    case 'tempfastmail':
      if (!token) throw new Error('internal error: token missing for tempfastmail');
      return tempfastmail.getEmails(token, email);
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
    case 'web-library-net':
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
    case 'imgui-de':
    case 'pulsewebmenu-de':
      return fakeLegal.getEmails(email);
    case 'moakt':
    case 'drmail-in':
    case 'teml-net':
    case 'tmpeml-com':
    case 'tmpbox-net':
    case 'moakt-cc':
    case 'disbox-net':
    case 'tmpmail-org':
    case 'tmpmail-net':
    case 'tmails-net':
    case 'disbox-org':
    case 'moakt-co':
    case 'moakt-ws':
    case 'tmail-ws':
    case 'bareed-ws':
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
    case 'jqkjqk-xyz':
    case 'lyhlevi-com':
      if (!token) throw new Error(`internal error: token missing for ${channel}`);
      return zhujump.getEmails(token, email);
    case 'tempmail-plus':
    case 'fexpost-com':
    case 'fexbox-org':
    case 'mailbox-in-ua':
    case 'rover-info':
    case 'chitthi-in':
    case 'fextemp-com':
    case 'any-pink':
    case 'merepost-com':
      return tempmailPlus.getEmails(email);
    case 'tempmail-lol-v2':
      if (!token) throw new Error('internal error: token missing for tempmail-lol-v2');
      return tempmailLolV2.getEmails(token, email);
    case 'tempgbox':
      if (!token) throw new Error('internal error: token missing for tempgbox');
      return tempgbox.getEmails(token, email);
    case 'emailnator':
      if (!token) throw new Error('internal error: token missing for emailnator');
      return emailnator.getEmails(token, email);
    case 'temporam':
      return temporam.getEmails(email);
    case 'mailinator':
      return mailinator.getEmails(token || '', email);
    case 'tempmail365':
      return tempmail365.getEmails(email);
    case 'neighbours':
      return neighbours.getEmails(email);
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
    case 'm2u':
      if (!token) throw new Error('internal error: token missing for m2u');
      return m2u.getEmails(token, email);
    case 'fmail':
      return fmail.getEmails(email);
    case 'ockito':
      if (!token) throw new Error('internal error: token missing for ockito');
      return ockito.getEmails(token, email);
    case 'anonbox':
      if (!token) throw new Error('internal error: token missing for anonbox');
      return anonbox.getEmails(token, email);
    case 'duckmail':
      if (!token) throw new Error('internal error: token missing for duckmail');
      return duckmail.getEmails(token, email);
    case 'tempy-email':
      return tempyEmail.getEmails(email);
    case 'tempinbox':
      return tempinbox.getEmails(email);
    case 'byom':
      return byom.getEmails(email);
    case 'anonymmail':
      return anonymmail.getEmails(email);
    case 'eyepaste':
      return eyepaste.getEmails(email);
    case 'segamail':
      if (!token) throw new Error('internal error: token missing for segamail');
      return segamail.getEmails(token, email);
    case 'mail-sunls':
      return mailSunls.getEmails(email);
    case 'expressinboxhub':
      if (!token) throw new Error('internal error: token missing for expressinboxhub');
      return expressinboxhub.getEmails(token, email);
    case 'lroid':
      if (!token) throw new Error('internal error: token missing for lroid');
      return lroid.getEmails(token, email);
    case 'haribu':
      if (!token) throw new Error('internal error: token missing for haribu');
      return haribu.getEmails(token, email);
    case 'pleasenospam':
      return pleasenospam.getEmails(email);
    case 'rootsh':
      if (!token) throw new Error('internal error: token missing for rootsh');
      return rootsh.getEmails(email, token);
    case 'fake-email-site':
      return fakeEmailSite.getEmails(email);
    case 'mohmal':
      if (!token) throw new Error('internal error: token missing for mohmal');
      return mohmal.getEmails(email, token);
    case 'mailgolem':
      if (!token) throw new Error('internal error: token missing for mailgolem');
      return mailgolem.getEmails(email, token);
    case 'best-temp-mail':
      if (!token) throw new Error('internal error: token missing for best-temp-mail');
      return bestTempMail.getEmails(email, token);
    case 'disposablemail-app':
      if (!token) throw new Error(`internal error: token missing for ${channel}`);
      return disposablemailApp.getEmails(token, email);
    case 'mailtemp-cc':
      if (!token) throw new Error('internal error: token missing for mailtemp-cc');
      return mailtempCc.getEmails(token, email);
    case 'minuteinbox':
      if (!token) throw new Error(`internal error: token missing for ${channel}`);
      return minuteinbox.getEmails(token, email);
    case 'tempemail-co':
      if (!token) throw new Error('internal error: token missing for tempemail-co');
      return tempemailCo.getEmails(token, email);
    case 'mailcatch':
      if (!token) throw new Error('internal error: token missing for mailcatch');
      return mailcatch.getEmails(token, email);
    case 'tempemails-net':
      if (!token) throw new Error(`internal error: token missing for ${channel}`);
      return tempemailsNet.getEmails(token, email);
    case 'altmails':
      if (!token) throw new Error('internal error: token missing for altmails');
      return altmails.getEmails(token, email);
    case 'tempemail-info':
      if (!token) throw new Error('internal error: token missing for tempemail-info');
      return tempemailInfo.getEmails(token, email);
    case 'smailpro':
      if (!token) throw new Error('internal error: token missing for smailpro');
      return smailpro.getEmails(token, email);
    case 'tempmailten':
      if (!token) throw new Error('internal error: token missing for tempmailten');
      return tempmailten.getEmails(token, email);
    case 'maildrop-cc':
      if (!token) throw new Error('internal error: token missing for maildrop-cc');
      return maildropCc.getEmails(token, email);
    case '10minutemail-net':
      if (!token) throw new Error('internal error: token missing for 10minutemail-net');
      return tenminutemailNet.getEmails(token, email);
    case 'linshiyouxiang-net':
      if (!token) throw new Error('internal error: token missing for linshiyouxiang-net');
      return linshiyouxiangNet.getEmails(token, email);
    case 'tempmail-fyi':
      if (!token) throw new Error('internal error: token missing for tempmail-fyi');
      return tempmailFyi.getEmails(token, email);
    case 'disposablemail-com':
      if (!token) throw new Error('internal error: token missing for disposablemail-com');
      return disposablemailCom.getEmails(token, email);
    case 'tempp-mails':
      if (!token) throw new Error('internal error: token missing for tempp-mails');
      return temppMails.getEmails(token, email);
    case 'emailtemp-org':
      if (!token) throw new Error('internal error: token missing for emailtemp-org');
      return emailtempOrg.getEmails(token, email);
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
