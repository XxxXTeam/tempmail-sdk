package tempemail

import (
	"fmt"
	"math/rand"
)

/* 所有支持的渠道列表，用于随机选择和遍历 */
var allChannels = []Channel{
	ChannelTempmail,
	ChannelLinshiEmail,
	ChannelLinshiyou,
	ChannelMffac,
	ChannelTempmailLol,
	ChannelChatgptOrgUk,
	ChannelTempMailIO,
	ChannelAwamail,
	ChannelTemporaryEmailOrg,
	ChannelMailTm,
	ChannelMailCx,
	ChannelDropmail,
	ChannelGuerrillaMail,
	ChannelMaildrop,
	ChannelSmailPw,
	ChannelBoomlify,
	ChannelMinmail,
	ChannelVip215,
	ChannelAnonbox,
	ChannelFakeLegal,
}

/*
 * ChannelInfo 渠道信息，包含渠道标识、显示名称和对应网站
 */
type ChannelInfo struct {
	/* 渠道标识 */
	Channel Channel
	/* 渠道显示名称 */
	Name string
	/* 对应的临时邮箱服务网站 */
	Website string
}

/* 渠道信息映射表 */
var channelInfoMap = map[Channel]ChannelInfo{
	ChannelTempmail:          {Channel: ChannelTempmail, Name: "TempMail", Website: "tempmail.ing"},
	ChannelLinshiEmail:       {Channel: ChannelLinshiEmail, Name: "临时邮箱", Website: "linshi-email.com"},
	ChannelLinshiyou:         {Channel: ChannelLinshiyou, Name: "临时邮", Website: "linshiyou.com"},
	ChannelMffac:             {Channel: ChannelMffac, Name: "MFFAC", Website: "mffac.com"},
	ChannelTempmailLol:       {Channel: ChannelTempmailLol, Name: "TempMail LOL", Website: "tempmail.lol"},
	ChannelChatgptOrgUk:      {Channel: ChannelChatgptOrgUk, Name: "ChatGPT Mail", Website: "mail.chatgpt.org.uk"},
	ChannelTempMailIO:        {Channel: ChannelTempMailIO, Name: "Temp Mail IO", Website: "temp-mail.io"},
	ChannelAwamail:           {Channel: ChannelAwamail, Name: "AwaMail", Website: "awamail.com"},
	ChannelTemporaryEmailOrg: {Channel: ChannelTemporaryEmailOrg, Name: "Temporary Email", Website: "temporary-email.org"},
	ChannelMailTm:            {Channel: ChannelMailTm, Name: "Mail.tm", Website: "mail.tm"},
	ChannelMailCx:            {Channel: ChannelMailCx, Name: "Mail.cx", Website: "mail.cx"},
	ChannelDropmail:          {Channel: ChannelDropmail, Name: "DropMail", Website: "dropmail.me"},
	ChannelGuerrillaMail:     {Channel: ChannelGuerrillaMail, Name: "Guerrilla Mail", Website: "guerrillamail.com"},
	ChannelMaildrop:          {Channel: ChannelMaildrop, Name: "Maildrop", Website: "maildrop.cx"},
	ChannelSmailPw:           {Channel: ChannelSmailPw, Name: "Smail.pw", Website: "smail.pw"},
	ChannelBoomlify:          {Channel: ChannelBoomlify, Name: "Boomlify", Website: "boomlify.com"},
	ChannelMinmail:           {Channel: ChannelMinmail, Name: "MinMail", Website: "minmail.app"},
	ChannelVip215:            {Channel: ChannelVip215, Name: "VIP 215", Website: "vip.215.im"},
	ChannelAnonbox:           {Channel: ChannelAnonbox, Name: "Anonbox", Website: "anonbox.net"},
	ChannelFakeLegal:         {Channel: ChannelFakeLegal, Name: "Fake Legal", Website: "fake.legal"},
}

/*
 * ListChannels 获取所有支持的渠道列表
 * 返回所有渠道的信息数组，包含渠道名称和对应网站
 */
func ListChannels() []ChannelInfo {
	result := make([]ChannelInfo, len(allChannels))
	for i, ch := range allChannels {
		result[i] = channelInfoMap[ch]
	}
	return result
}

/*
 * GetChannelInfo 获取指定渠道的详细信息
 * 返回渠道信息和是否存在的标记
 */
func GetChannelInfo(channel Channel) (ChannelInfo, bool) {
	info, ok := channelInfoMap[channel]
	return info, ok
}

/*
 * GenerateEmail 创建临时邮箱
 *
 * 错误处理策略:
 * - 指定渠道失败时，自动尝试其他可用渠道（打乱顺序逐个尝试）
 * - 未指定渠道时，打乱全部渠道逐个尝试，直到成功
 * - 所有渠道均不可用时返回 nil（不返回 error）
 *
 * 示例:
 *   info, _ := GenerateEmail(&GenerateEmailOptions{Channel: ChannelTempMailIO})
 *   if info != nil { fmt.Println(info.Email) }
 */
func GenerateEmail(opts *GenerateEmailOptions) (*EmailInfo, error) {
	if opts == nil {
		opts = &GenerateEmailOptions{}
	}

	/*
	 * 构建尝试顺序：
	 * - 指定渠道 → 优先尝试该渠道，失败后随机尝试其他渠道
	 * - 未指定 → 打乱全部渠道逐个尝试
	 */
	tryOrder := buildChannelOrder(opts.Channel)

	for _, ch := range tryOrder {
		sdkLogger.Info("创建临时邮箱", "channel", string(ch))
		result, err := WithRetry(func() (*EmailInfo, error) {
			return generateEmailOnce(ch, opts)
		}, opts.Retry)
		if err == nil && result != nil {
			sdkLogger.Info("邮箱创建成功", "channel", string(ch), "email", result.Email)
			return result, nil
		}
		errMsg := "unknown error"
		if err != nil {
			errMsg = err.Error()
		}
		sdkLogger.Warn("渠道不可用，尝试下一个", "channel", string(ch), "error", errMsg)
	}

	sdkLogger.Error("所有渠道均不可用，创建邮箱失败")
	return nil, nil
}

/*
 * buildChannelOrder 构建渠道尝试顺序
 * 指定渠道时优先尝试该渠道，其余渠道打乱追加
 * 未指定时打乱全部渠道
 */
func buildChannelOrder(preferred Channel) []Channel {
	shuffled := make([]Channel, len(allChannels))
	copy(shuffled, allChannels)
	rand.Shuffle(len(shuffled), func(i, j int) {
		shuffled[i], shuffled[j] = shuffled[j], shuffled[i]
	})
	if preferred == "" {
		return shuffled
	}
	result := []Channel{preferred}
	for _, ch := range shuffled {
		if ch != preferred {
			result = append(result, ch)
		}
	}
	return result
}

/*
 * generateEmailOnce 单次创建邮箱（不含重试逻辑）
 * 根据渠道类型分发到对应的 provider 实现
 */
func generateEmailOnce(channel Channel, opts *GenerateEmailOptions) (*EmailInfo, error) {
	switch channel {
	case ChannelTempmail:
		duration := opts.Duration
		if duration <= 0 {
			duration = 30
		}
		return tempmailGenerate(duration)

	case ChannelLinshiEmail:
		return linshiEmailGenerate()

	case ChannelLinshiyou:
		return linshiyouGenerate()

	case ChannelMffac:
		return mffacGenerate()

	case ChannelTempmailLol:
		return tempmailLolGenerate(opts.Domain)

	case ChannelChatgptOrgUk:
		return chatgptOrgUkGenerate()

	case ChannelTempMailIO:
		return tempMailIOGenerate()

	case ChannelAwamail:
		return awamailGenerate()

	case ChannelTemporaryEmailOrg:
		return temporaryEmailOrgGenerate()

	case ChannelMailTm:
		return mailTmGenerate()

	case ChannelMailCx:
		return mailCxGenerate(opts)

	case ChannelDropmail:
		return dropmailGenerate()

	case ChannelGuerrillaMail:
		return guerrillaMailGenerate()

	case ChannelMaildrop:
		return maildropGenerate(opts.Domain)

	case ChannelSmailPw:
		return smailPwGenerate()

	case ChannelBoomlify:
		return boomlifyGenerate()

	case ChannelMinmail:
		return minmailGenerate()

	case ChannelVip215:
		return mailVip215Generate()

	case ChannelAnonbox:
		return anonboxGenerate()

	case ChannelFakeLegal:
		return fakeLegalGenerate(opts.Domain)

	default:
		return nil, fmt.Errorf("unknown channel: %s", channel)
	}
}

/*
 * GetEmails 获取邮件列表
 * Channel/Email/Token 等信息由 SDK 从 EmailInfo 中自动获取，用户无需手动传递
 *
 * 错误处理策略:
 * - 网络错误、超时、服务端 5xx 错误 → 自动重试（默认 2 次）
 * - 重试耗尽后返回 { Success: false, Emails: [] }，不返回 error
 * - 参数校验错误（缺少 EmailInfo）直接返回 error
 *
 * 这种设计让调用方在轮询场景下不会因网络波动而中断整个流程，
 * 只需检查 Success 字段即可判断本次请求是否成功。
 *
 * 示例:
 *   info, _ := GenerateEmail(&GenerateEmailOptions{Channel: ChannelMailTm})
 *   result, _ := GetEmails(info, nil)
 */
func GetEmails(info *EmailInfo, opts *GetEmailsOptions) (*GetEmailsResult, error) {
	if info == nil {
		return nil, fmt.Errorf("EmailInfo is required, call GenerateEmail() first")
	}
	if info.Channel == "" {
		return nil, fmt.Errorf("channel is required")
	}
	if info.Email == "" && info.Channel != ChannelTempmailLol {
		return nil, fmt.Errorf("email is required")
	}

	var retry *RetryOptions
	if opts != nil {
		retry = opts.Retry
	}

	sdkLogger.Debug("获取邮件", "channel", string(info.Channel), "email", info.Email)
	emails, err := WithRetry(func() ([]Email, error) {
		return getEmailsOnce(info.Channel, info.Email, info.token)
	}, retry)

	if err != nil {
		/*
		 * 重试耗尽后仍然失败 → 返回空结果而非 error
		 * 这样调用方在轮询场景下不会因为一次网络波动而中断整个流程
		 */
		sdkLogger.Error("获取邮件失败", "channel", string(info.Channel), "error", err.Error())
		return &GetEmailsResult{
			Channel: info.Channel,
			Email:   info.Email,
			Emails:  []Email{},
			Success: false,
		}, nil
	}

	if len(emails) > 0 {
		sdkLogger.Info("获取到邮件", "channel", string(info.Channel), "count", len(emails))
	} else {
		sdkLogger.Debug("暂无邮件", "channel", string(info.Channel))
	}

	return &GetEmailsResult{
		Channel: info.Channel,
		Email:   info.Email,
		Emails:  emails,
		Success: true,
	}, nil
}

/*
 * getEmailsOnce 单次获取邮件（不含重试逻辑）
 * 根据渠道类型分发到对应的 provider 实现
 * token 由 SDK 内部从 EmailInfo 中获取，用户无感知
 */
func getEmailsOnce(channel Channel, email string, token string) ([]Email, error) {
	switch channel {
	case ChannelTempmail:
		return tempmailGetEmails(email)

	case ChannelLinshiEmail:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for linshi-email channel")
		}
		return linshiEmailGetEmails(token, email)

	case ChannelLinshiyou:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for linshiyou channel")
		}
		return linshiyouGetEmails(token, email)

	case ChannelMffac:
		return mffacGetEmails(email, token)

	case ChannelTempmailLol:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for tempmail-lol channel")
		}
		return tempmailLolGetEmails(token, email)

	case ChannelChatgptOrgUk:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for chatgpt-org-uk channel")
		}
		return chatgptOrgUkGetEmails(email, token)

	case ChannelTempMailIO:
		return tempMailIOGetEmails(email)

	case ChannelAwamail:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for awamail channel")
		}
		return awamailGetEmails(token, email)

	case ChannelTemporaryEmailOrg:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for temporary-email-org channel")
		}
		return temporaryEmailOrgGetEmails(token, email)

	case ChannelMailTm:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for mail-tm channel")
		}
		return mailTmGetEmails(token, email)

	case ChannelMailCx:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for mail-cx channel")
		}
		return mailCxGetEmails(token, email)

	case ChannelDropmail:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for dropmail channel")
		}
		return dropmailGetEmails(token, email)

	case ChannelGuerrillaMail:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for guerrillamail channel")
		}
		return guerrillaMailGetEmails(token, email)

	case ChannelMaildrop:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for maildrop channel")
		}
		return maildropGetEmails(token, email)

	case ChannelSmailPw:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for smail-pw channel")
		}
		return smailPwGetEmails(token, email)

	case ChannelBoomlify:
		return boomlifyGetEmails(email)

	case ChannelMinmail:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for minmail channel")
		}
		return minmailGetEmails(email, token)

	case ChannelVip215:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for vip-215 channel")
		}
		return mailVip215GetEmails(token, email)

	case ChannelAnonbox:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for anonbox channel")
		}
		return anonboxGetEmails(token, email)

	case ChannelFakeLegal:
		return fakeLegalGetEmails(email)

	default:
		return nil, fmt.Errorf("unknown channel: %s", channel)
	}
}

/*
 * EmailInfo.GetEmails 获取当前邮箱的邮件列表
 * Token 等内部信息由 SDK 自动维护，用户无需关心
 *
 * 示例:
 *   info, _ := GenerateEmail(&GenerateEmailOptions{Channel: ChannelMailTm})
 *   result, _ := info.GetEmails(nil)
 */
func (info *EmailInfo) GetEmails(opts *GetEmailsOptions) (*GetEmailsResult, error) {
	return GetEmails(info, opts)
}

/*
 * Client 临时邮箱客户端
 * 封装了邮箱创建和邮件获取的完整流程，自动管理邮箱信息和认证令牌
 *
 * 示例:
 *   client := NewClient()
 *   info, _ := client.Generate(&GenerateEmailOptions{Channel: ChannelMailTm})
 *   result, _ := client.GetEmails(nil)
 *   if result.Success {
 *       fmt.Println("邮件数:", len(result.Emails))
 *   }
 */
type Client struct {
	emailInfo *EmailInfo
}

/* NewClient 创建临时邮箱客户端实例 */
func NewClient() *Client {
	return &Client{}
}

/*
 * Generate 创建临时邮箱并缓存邮箱信息
 * 后续调用 GetEmails() 时自动使用此邮箱的渠道、地址和令牌
 */
func (c *Client) Generate(opts *GenerateEmailOptions) (*EmailInfo, error) {
	info, err := GenerateEmail(opts)
	if err != nil {
		return nil, err
	}
	c.emailInfo = info
	return info, nil
}

/*
 * GetEmails 获取当前邮箱的邮件列表
 * 必须先调用 Generate() 创建邮箱
 */
func (c *Client) GetEmails(opts *GetEmailsOptions) (*GetEmailsResult, error) {
	if c.emailInfo == nil {
		return nil, fmt.Errorf("no email generated. Call Generate() first")
	}

	return GetEmails(c.emailInfo, opts)
}

/* GetEmailInfo 获取当前缓存的邮箱信息，未调用 Generate() 时返回 nil */
func (c *Client) GetEmailInfo() *EmailInfo {
	return c.emailInfo
}
