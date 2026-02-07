package tempemail

// Channel 渠道类型
type Channel string

const (
	ChannelTempmail     Channel = "tempmail"
	ChannelLinshiEmail  Channel = "linshi-email"
	ChannelTempmailLol  Channel = "tempmail-lol"
	ChannelChatgptOrgUk Channel = "chatgpt-org-uk"
	ChannelTempmailLa   Channel = "tempmail-la"
	ChannelTempMailIO   Channel = "temp-mail-io"
	ChannelAwamail      Channel = "awamail"
	ChannelMailTm       Channel = "mail-tm"
	ChannelDropmail     Channel = "dropmail"
)

// EmailInfo 生成邮箱时返回的信息
type EmailInfo struct {
	Channel   Channel `json:"channel"`
	Email     string  `json:"email"`
	Token     string  `json:"token,omitempty"`
	ExpiresAt any     `json:"expiresAt,omitempty"`
	CreatedAt string  `json:"createdAt,omitempty"`
}

// EmailAttachment 标准化邮件附件
type EmailAttachment struct {
	Filename    string `json:"filename"`
	Size        int64  `json:"size,omitempty"`
	ContentType string `json:"contentType,omitempty"`
	URL         string `json:"url,omitempty"`
}

// Email 标准化邮件格式 - 所有提供商返回统一结构
type Email struct {
	// 邮件唯一标识
	ID string `json:"id"`
	// 发件人邮箱地址
	From string `json:"from"`
	// 收件人邮箱地址
	To string `json:"to"`
	// 邮件主题
	Subject string `json:"subject"`
	// 纯文本内容
	Text string `json:"text"`
	// HTML 内容
	HTML string `json:"html"`
	// ISO 8601 格式的日期字符串
	Date string `json:"date"`
	// 是否已读
	IsRead bool `json:"isRead"`
	// 附件列表
	Attachments []EmailAttachment `json:"attachments"`
}

// GetEmailsResult 获取邮件的统一返回结果
type GetEmailsResult struct {
	Channel Channel `json:"channel"`
	Email   string  `json:"email"`
	Emails  []Email `json:"emails"`
	Success bool    `json:"success"`
}

// GenerateEmailOptions 生成邮箱的选项
type GenerateEmailOptions struct {
	Channel  Channel
	Duration int
	Domain   *string
}

// GetEmailsOptions 获取邮件的选项
type GetEmailsOptions struct {
	Channel Channel
	Email   string
	Token   string
}
