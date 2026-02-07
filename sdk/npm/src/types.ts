export type Channel = 'tempmail' | 'linshi-email' | 'tempmail-lol' | 'chatgpt-org-uk' | 'tempmail-la' | 'temp-mail-io' | 'awamail' | 'mail-tm' | 'dropmail';

export interface EmailInfo {
  channel: Channel;
  email: string;
  token?: string;
  expiresAt?: string | number;
  createdAt?: string;
}

/**
 * 标准化邮件附件
 */
export interface EmailAttachment {
  filename: string;
  size?: number;
  contentType?: string;
  url?: string;
}

/**
 * 标准化邮件格式 - 所有提供商返回统一结构
 */
export interface Email {
  /** 邮件唯一标识 */
  id: string;
  /** 发件人邮箱地址 */
  from: string;
  /** 收件人邮箱地址 */
  to: string;
  /** 邮件主题 */
  subject: string;
  /** 纯文本内容 */
  text: string;
  /** HTML 内容 */
  html: string;
  /** ISO 8601 格式的日期字符串 */
  date: string;
  /** 是否已读 */
  isRead: boolean;
  /** 附件列表 */
  attachments: EmailAttachment[];
}

export interface GetEmailsResult {
  channel: Channel;
  email: string;
  emails: Email[];
  success: boolean;
}

export interface GenerateEmailOptions {
  channel?: Channel;
  duration?: number;
  domain?: string | null;
}

export interface GetEmailsOptions {
  channel: Channel;
  email: string;
  token?: string;
}
