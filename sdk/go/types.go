package tempemail

/*
 * Channel 支持的临时邮箱渠道标识
 * 每个渠道对应一个第三方临时邮箱服务商
 */
type Channel string

const (
	ChannelTempmail      Channel = "tempmail"       // tempmail.ing
	ChannelLinshiEmail   Channel = "linshi-email"   // linshi-email.com
	ChannelTempmailLol   Channel = "tempmail-lol"   // tempmail.lol
	ChannelChatgptOrgUk  Channel = "chatgpt-org-uk" // mail.chatgpt.org.uk
	ChannelTempmailLa    Channel = "tempmail-la"    // tempmail.la
	ChannelTempMailIO    Channel = "temp-mail-io"   // temp-mail.io
	ChannelAwamail       Channel = "awamail"        // awamail.com
	ChannelMailTm        Channel = "mail-tm"        // mail.tm
	ChannelDropmail      Channel = "dropmail"       // dropmail.me
	ChannelGuerrillaMail Channel = "guerrillamail"  // guerrillamail.com
	ChannelMaildrop      Channel = "maildrop"       // maildrop.cc
)

/*
 * EmailInfo 创建临时邮箱后返回的邮箱信息
 * 包含邮箱地址、认证令牌和生命周期信息
 */
type EmailInfo struct {
	/* 创建该邮箱所使用的渠道 */
	Channel Channel `json:"channel"`
	/* 临时邮箱地址 */
	Email string `json:"email"`
	/* 认证令牌，部分渠道在获取邮件时需要此令牌 */
	Token string `json:"token,omitempty"`
	/* 邮箱过期时间（ISO 8601 字符串或 Unix 时间戳） */
	ExpiresAt any `json:"expiresAt,omitempty"`
	/* 邮箱创建时间（ISO 8601 字符串） */
	CreatedAt string `json:"createdAt,omitempty"`
}

/*
 * EmailAttachment 标准化邮件附件
 * 不同渠道的附件字段名不同，SDK 统一归一化为此结构
 */
type EmailAttachment struct {
	/* 附件文件名 */
	Filename string `json:"filename"`
	/* 附件大小（字节） */
	Size int64 `json:"size,omitempty"`
	/* MIME 类型，如 application/pdf */
	ContentType string `json:"contentType,omitempty"`
	/* 附件下载地址 */
	URL string `json:"url,omitempty"`
}

/*
 * Email 标准化邮件格式
 * 所有渠道的 API 响应经过归一化处理后统一返回此结构
 */
type Email struct {
	/* 邮件唯一标识 */
	ID string `json:"id"`
	/* 发件人邮箱地址 */
	From string `json:"from"`
	/* 收件人邮箱地址 */
	To string `json:"to"`
	/* 邮件主题 */
	Subject string `json:"subject"`
	/* 纯文本内容 */
	Text string `json:"text"`
	/* HTML 内容 */
	HTML string `json:"html"`
	/* ISO 8601 格式的日期字符串 */
	Date string `json:"date"`
	/* 是否已读 */
	IsRead bool `json:"isRead"`
	/* 附件列表 */
	Attachments []EmailAttachment `json:"attachments"`
}

/*
 * GetEmailsResult 获取邮件列表的返回结果
 * Success 为 false 时表示请求失败（重试耗尽），Emails 为空切片
 */
type GetEmailsResult struct {
	/* 所使用的渠道 */
	Channel Channel `json:"channel"`
	/* 查询的邮箱地址 */
	Email string `json:"email"`
	/* 邮件列表，失败时为空切片 */
	Emails []Email `json:"emails"`
	/* 请求是否成功，false 表示重试耗尽后仍失败 */
	Success bool `json:"success"`
}

/*
 * GenerateEmailOptions 创建临时邮箱的选项
 *
 * 示例:
 *   info, err := GenerateEmail(&GenerateEmailOptions{
 *       Channel: ChannelMailTm,
 *       Retry:   &RetryOptions{MaxRetries: 3},
 *   })
 */
type GenerateEmailOptions struct {
	/* 指定渠道，空字符串则随机选择 */
	Channel Channel
	/* 邮箱有效时长（分钟），仅 tempmail 渠道支持，默认 30 */
	Duration int
	/* 指定邮箱域名，仅 tempmail-lol 渠道支持 */
	Domain *string
	/* 重试配置，nil 则使用默认值（最多重试 2 次） */
	Retry *RetryOptions
}

/*
 * GetEmailsOptions 获取邮件列表的选项
 *
 * 示例:
 *   result, err := GetEmails(GetEmailsOptions{
 *       Channel: info.Channel,
 *       Email:   info.Email,
 *       Token:   info.Token,
 *   })
 *   if result.Success && len(result.Emails) > 0 {
 *       fmt.Println("收到邮件:", result.Emails[0].Subject)
 *   }
 */
type GetEmailsOptions struct {
	/* 渠道标识，必填 */
	Channel Channel
	/* 邮箱地址，必填 */
	Email string
	/* 认证令牌，mail-tm / awamail / dropmail / tempmail-lol 渠道必填 */
	Token string
	/* 重试配置，nil 则使用默认值（最多重试 2 次） */
	Retry *RetryOptions
}
