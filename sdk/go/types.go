package tempemail

/*
 * Channel 支持的临时邮箱渠道标识
 * 每个渠道对应一个第三方临时邮箱服务商。
 */
type Channel string

const (
	ChannelTempmail              Channel = "tempmail"                // tempmail.ing
	ChannelTempmailCn            Channel = "tempmail-cn"             // tempmail.cn
	ChannelTaEasy                Channel = "ta-easy"                 // ta-easy.com
	Channel10minuteOne           Channel = "10minute-one"            // 10minutemail.one（SSR JWT + web API）
	ChannelLinshiyou             Channel = "linshiyou"               // linshiyou.com
	ChannelMffac                 Channel = "mffac"                   // mffac.com
	ChannelTempmailLol           Channel = "tempmail-lol"            // tempmail.lol
	ChannelChatgptOrgUk          Channel = "chatgpt-org-uk"          // mail.chatgpt.org.uk
	ChannelTempMailIo            Channel = "temp-mail-io"            // temp-mail.io
	ChannelMailCx                Channel = "mail-cx"                 // mail.cx
	ChannelCatchmail             Channel = "catchmail"               // catchmail.io
	ChannelCatchmailMailistry    Channel = "catchmail-mailistry"     // mailistry.com（Catchmail API 域名）
	ChannelCatchmailZeppost      Channel = "catchmail-zeppost"       // zeppost.com（Catchmail API 域名）
	ChannelMailforspam           Channel = "mailforspam"             // mailforspam.com
	ChannelMailforspamTempmailIo Channel = "mailforspam-tempmail-io" // tempmail.io（MailForSpam API 域名）
	ChannelMailforspamDisposable Channel = "mailforspam-disposable"  // disposable.email（MailForSpam API 域名）
	ChannelTempmailo             Channel = "tempmailo"               // tempmailo.com
	ChannelTempmailc             Channel = "tempmailc"               // tempmailc.com
	ChannelMailnesia             Channel = "mailnesia"               // mailnesia.com
	ChannelThrowawaymail         Channel = "throwawaymail"           // throwawaymail.app
	ChannelInboxkitten           Channel = "inboxkitten"             // inboxkitten.com
	ChannelGetnada               Channel = "getnada"                 // getnada.net
	ChannelMail123               Channel = "mail123"                 // mail123.fr
	ChannelOneSecMail            Channel = "1sec-mail"               // 1sec-mail.com
	ChannelFakemail              Channel = "fakemail"                // fakemail.net
	ChannelOpeninbox             Channel = "openinbox"               // openinbox.io
	ChannelInboxes               Channel = "inboxes"                 // inboxes.com
	ChannelUncorreotemporal      Channel = "uncorreotemporal"        // uncorreotemporal.com
	ChannelAwamail               Channel = "awamail"                 // awamail.com
	ChannelMailTm                Channel = "mail-tm"                 // mail.tm
	ChannelDropmail              Channel = "dropmail"                // dropmail.me
	ChannelGuerrillaMail         Channel = "guerrillamail"           // guerrillamail.com
	ChannelGuerrillamailCom      Channel = "guerrillamail-com"       // guerrillamail.com 裸域 JSON API
	ChannelMaildrop              Channel = "maildrop"                // maildrop.cx
	ChannelSmailPw               Channel = "smail-pw"                // smail.pw
	ChannelVip215                Channel = "vip-215"                 // vip.215.im
	ChannelFakeLegal             Channel = "fake-legal"              // fake.legal
	ChannelMoakt                 Channel = "moakt"                   // moakt.com（HTML 收件箱 + tm_session Cookie）
	ChannelEmail10min            Channel = "email10min"              // email10min.com（CSRF + Cookie）
	ChannelMjjCm                 Channel = "mjj-cm"                  // mjj.cm（Socket.IO）
	ChannelLinshiCo              Channel = "linshi-co"               // linshi.co（Socket.IO）
	ChannelHarakirimail          Channel = "harakirimail"            // harakirimail.com
	ChannelTempmailPlus          Channel = "tempmail-plus"           // tempmail.plus
	ChannelMailGw                Channel = "mail-gw"                 // mail.gw（JWT 认证 REST API）
	ChannelTempmailLolV2         Channel = "tempmail-lol-v2"         // tempmail.lol（V2 REST API）
	ChannelSharklasers           Channel = "sharklasers"             // sharklasers.com（Guerrillamail 镜像）
	ChannelSharklasersCom        Channel = "sharklasers-com"         // sharklasers.com 裸域镜像
	ChannelGrrLa                 Channel = "grr-la"                  // grr.la（Guerrillamail 镜像）
	ChannelGrrLaCom              Channel = "grr-la-com"              // grr.la 裸域镜像
	ChannelGuerrillamailInfo     Channel = "guerrillamail-info"      // guerrillamail.info（Guerrillamail 镜像）
	ChannelSpam4me               Channel = "spam4me"                 // spam4.me（Guerrillamail 镜像）
	ChannelGuerrillamailNet      Channel = "guerrillamail-net"       // guerrillamail.net（Guerrillamail 镜像）
	ChannelGuerrillamailOrg      Channel = "guerrillamail-org"       // guerrillamail.org（Guerrillamail 镜像）
	ChannelGuerrillamailBlock    Channel = "guerrillamailblock"      // guerrillamailblock.com（Guerrillamail 镜像）
	ChannelGuerrillamailComWww   Channel = "guerrillamail-com-www"   // guerrillamail.com（Guerrillamail 镜像）
)

/*
 * EmailInfo 创建临时邮箱后返回的邮箱信息
 * 包含邮箱地址和生命周期信息，认证令牌由 SDK 内部维护，不对外暴露
 */
type EmailInfo struct {
	/* 创建该邮箱所使用的渠道 */
	Channel Channel `json:"channel"`
	/* 临时邮箱地址 */
	Email string `json:"email"`
	/* 认证令牌，由 SDK 内部维护，不对外暴露 */
	token string
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
	/* 指定邮箱域名/接入域名：如 tempmail-cn、tempmail-lol、fake-legal、catchmail、mailforspam；moakt 渠道表示语言路径（如 zh、en） */
	Domain *string
	/* 重试配置，nil 则使用默认值（最多重试 2 次） */
	Retry *RetryOptions
}

/*
 * GetEmailsOptions 获取邮件列表的选项
 * Channel/Email/Token 等由 SDK 从 EmailInfo 中自动获取，用户无需手动传递
 *
 * 示例:
 *   info, _ := GenerateEmail(&GenerateEmailOptions{Channel: ChannelMailTm})
 *   result, _ := GetEmails(info, nil) // nil 表示使用默认配置
 *   if result.Success && len(result.Emails) > 0 {
 *       fmt.Println("收到邮件:", result.Emails[0].Subject)
 *   }
 */
type GetEmailsOptions struct {
	/* 重试配置，nil 则使用默认值（最多重试 2 次） */
	Retry *RetryOptions
}
