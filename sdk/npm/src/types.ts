/**
 * 支持的临时邮箱渠道标识
 * 每个渠道对应一个第三方临时邮箱服务商
 */
export type Channel = 'tempmail' | 'linshi-email' | 'tempmail-lol' | 'chatgpt-org-uk' | 'tempmail-la' | 'temp-mail-io' | 'awamail' | 'mail-tm' | 'dropmail';

/**
 * 创建临时邮箱后返回的邮箱信息
 * 包含邮箱地址、认证令牌和生命周期信息
 */
export interface EmailInfo {
  /** 创建该邮箱所使用的渠道 */
  channel: Channel;
  /** 临时邮箱地址 */
  email: string;
  /** 认证令牌，部分渠道在获取邮件时需要此令牌 */
  token?: string;
  /** 邮箱过期时间（ISO 8601 字符串或 Unix 时间戳） */
  expiresAt?: string | number;
  /** 邮箱创建时间（ISO 8601 字符串） */
  createdAt?: string;
}

/**
 * 标准化邮件附件
 * 不同渠道的附件字段名不同，SDK 统一归一化为此结构
 */
export interface EmailAttachment {
  /** 附件文件名 */
  filename: string;
  /** 附件大小（字节） */
  size?: number;
  /** MIME 类型，如 application/pdf */
  contentType?: string;
  /** 附件下载地址 */
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

/**
 * 获取邮件列表的返回结果
 * success 为 false 时表示请求失败（重试耗尽），emails 为空数组
 */
export interface GetEmailsResult {
  /** 所使用的渠道 */
  channel: Channel;
  /** 查询的邮箱地址 */
  email: string;
  /** 邮件列表，失败时为空数组 */
  emails: Email[];
  /** 请求是否成功，false 表示重试耗尽后仍失败 */
  success: boolean;
}

/**
 * 重试配置
 * SDK 内部对网络错误、超时、5xx 服务端错误自动重试
 * 4xx 客户端错误（如参数错误）不会重试
 *
 * @example
 * ```ts
 * // 自定义重试策略：最多重试 3 次，首次延迟 2 秒
 * const email = await generateEmail({
 *   channel: 'mail-tm',
 *   retry: { maxRetries: 3, initialDelay: 2000 },
 * });
 * ```
 */
export interface RetryConfig {
  /** 最大重试次数（不含首次请求），默认 2 */
  maxRetries?: number;
  /** 初始重试延迟（毫秒），采用指数退避策略，默认 1000 */
  initialDelay?: number;
  /** 最大重试延迟上限（毫秒），默认 5000 */
  maxDelay?: number;
  /** 单次请求超时时间（毫秒），默认 15000 */
  timeout?: number;
}

/**
 * 创建临时邮箱的选项
 *
 * @example
 * ```ts
 * // 使用指定渠道创建邮箱
 * const email = await generateEmail({ channel: 'mail-tm' });
 *
 * // 随机选择渠道
 * const email = await generateEmail();
 * ```
 */
export interface GenerateEmailOptions {
  /** 指定渠道，不传则随机选择 */
  channel?: Channel;
  /** 邮箱有效时长 */
  duration?: number;
  /** 指定邮箱域名 */
  domain?: string | null;
  /** 重试配置，不传则使用默认值（最多重试 2 次） */
  retry?: RetryConfig;
}

/**
 * 获取邮件列表的选项
 *
 * @example
 * ```ts
 * const result = await getEmails({
 *   channel: emailInfo.channel,
 *   email: emailInfo.email,
 *   token: emailInfo.token,
 * });
 * if (result.success && result.emails.length > 0) {
 *   console.log('收到邮件:', result.emails);
 * }
 * ```
 */
export interface GetEmailsOptions {
  /** 渠道标识，必填 */
  channel: Channel;
  /** 邮箱地址，必填 */
  email: string;
  /** 认证令牌 */
  token?: string;
  /** 重试配置，不传则使用默认值（最多重试 2 次） */
  retry?: RetryConfig;
}
