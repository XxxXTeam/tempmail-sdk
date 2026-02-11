package tempemail

import (
	"fmt"
	"math/rand"
	"time"
)

/* 所有支持的渠道列表，用于随机选择和遍历 */
var allChannels = []Channel{
	ChannelTempmail,
	ChannelLinshiEmail,
	ChannelTempmailLol,
	ChannelChatgptOrgUk,
	ChannelTempmailLa,
	ChannelTempMailIO,
	ChannelAwamail,
	ChannelMailTm,
	ChannelDropmail,
	ChannelGuerrillaMail,
	ChannelMaildrop,
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
	ChannelTempmail:      {Channel: ChannelTempmail, Name: "TempMail", Website: "tempmail.ing"},
	ChannelLinshiEmail:   {Channel: ChannelLinshiEmail, Name: "临时邮箱", Website: "linshi-email.com"},
	ChannelTempmailLol:   {Channel: ChannelTempmailLol, Name: "TempMail LOL", Website: "tempmail.lol"},
	ChannelChatgptOrgUk:  {Channel: ChannelChatgptOrgUk, Name: "ChatGPT Mail", Website: "mail.chatgpt.org.uk"},
	ChannelTempmailLa:    {Channel: ChannelTempmailLa, Name: "TempMail LA", Website: "tempmail.la"},
	ChannelTempMailIO:    {Channel: ChannelTempMailIO, Name: "Temp Mail IO", Website: "temp-mail.io"},
	ChannelAwamail:       {Channel: ChannelAwamail, Name: "AwaMail", Website: "awamail.com"},
	ChannelMailTm:        {Channel: ChannelMailTm, Name: "Mail.tm", Website: "mail.tm"},
	ChannelDropmail:      {Channel: ChannelDropmail, Name: "DropMail", Website: "dropmail.me"},
	ChannelGuerrillaMail: {Channel: ChannelGuerrillaMail, Name: "Guerrilla Mail", Website: "guerrillamail.com"},
	ChannelMaildrop:      {Channel: ChannelMaildrop, Name: "Maildrop", Website: "maildrop.cc"},
}

func init() {
	rand.Seed(time.Now().UnixNano())
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
 * - 网络错误、超时、服务端 5xx 错误 → 自动重试（默认 2 次，指数退避）
 * - 4xx 客户端错误、参数错误 → 直接返回 error
 * - 重试耗尽后仍失败时返回 error
 *
 * 示例:
 *   info, err := GenerateEmail(&GenerateEmailOptions{Channel: ChannelTempMailIO})
 *   if err != nil { log.Fatal(err) }
 *   fmt.Println(info.Email)
 */
func GenerateEmail(opts *GenerateEmailOptions) (*EmailInfo, error) {
	if opts == nil {
		opts = &GenerateEmailOptions{}
	}

	channel := opts.Channel
	if channel == "" {
		channel = allChannels[rand.Intn(len(allChannels))]
	}

	sdkLogger.Info("创建临时邮箱", "channel", string(channel))
	result, err := WithRetry(func() (*EmailInfo, error) {
		return generateEmailOnce(channel, opts)
	}, opts.Retry)
	if err != nil {
		sdkLogger.Error("创建邮箱失败", "channel", string(channel), "error", err.Error())
		return nil, err
	}
	sdkLogger.Info("邮箱创建成功", "channel", string(channel), "email", result.Email)
	return result, nil
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

	case ChannelTempmailLol:
		return tempmailLolGenerate(opts.Domain)

	case ChannelChatgptOrgUk:
		return chatgptOrgUkGenerate()

	case ChannelTempmailLa:
		return tempmailLaGenerate()

	case ChannelTempMailIO:
		return tempMailIOGenerate()

	case ChannelAwamail:
		return awamailGenerate()

	case ChannelMailTm:
		return mailTmGenerate()

	case ChannelDropmail:
		return dropmailGenerate()

	case ChannelGuerrillaMail:
		return guerrillaMailGenerate()

	case ChannelMaildrop:
		return maildropGenerate()

	default:
		return nil, fmt.Errorf("unknown channel: %s", channel)
	}
}

/*
 * GetEmails 获取邮件列表
 *
 * 错误处理策略:
 * - 网络错误、超时、服务端 5xx 错误 → 自动重试（默认 2 次）
 * - 重试耗尽后返回 { Success: false, Emails: [] }，不返回 error
 * - 参数校验错误（缺少 Channel/Token）直接返回 error
 *
 * 这种设计让调用方在轮询场景下不会因网络波动而中断整个流程，
 * 只需检查 Success 字段即可判断本次请求是否成功。
 */
func GetEmails(opts GetEmailsOptions) (*GetEmailsResult, error) {
	if opts.Channel == "" {
		return nil, fmt.Errorf("channel is required")
	}
	if opts.Email == "" && opts.Channel != ChannelTempmailLol {
		return nil, fmt.Errorf("email is required")
	}

	sdkLogger.Debug("获取邮件", "channel", string(opts.Channel), "email", opts.Email)
	emails, err := WithRetry(func() ([]Email, error) {
		return getEmailsOnce(opts)
	}, opts.Retry)

	if err != nil {
		/*
		 * 重试耗尽后仍然失败 → 返回空结果而非 error
		 * 这样调用方在轮询场景下不会因为一次网络波动而中断整个流程
		 */
		sdkLogger.Error("获取邮件失败", "channel", string(opts.Channel), "error", err.Error())
		return &GetEmailsResult{
			Channel: opts.Channel,
			Email:   opts.Email,
			Emails:  []Email{},
			Success: false,
		}, nil
	}

	if len(emails) > 0 {
		sdkLogger.Info("获取到邮件", "channel", string(opts.Channel), "count", len(emails))
	} else {
		sdkLogger.Debug("暂无邮件", "channel", string(opts.Channel))
	}

	return &GetEmailsResult{
		Channel: opts.Channel,
		Email:   opts.Email,
		Emails:  emails,
		Success: true,
	}, nil
}

/*
 * getEmailsOnce 单次获取邮件（不含重试逻辑）
 * 根据渠道类型分发到对应的 provider 实现，并校验必需的 Token 参数
 */
func getEmailsOnce(opts GetEmailsOptions) ([]Email, error) {
	switch opts.Channel {
	case ChannelTempmail:
		return tempmailGetEmails(opts.Email)

	case ChannelLinshiEmail:
		return linshiEmailGetEmails(opts.Email)

	case ChannelTempmailLol:
		if opts.Token == "" {
			return nil, fmt.Errorf("token is required for tempmail-lol channel")
		}
		return tempmailLolGetEmails(opts.Token, opts.Email)

	case ChannelChatgptOrgUk:
		return chatgptOrgUkGetEmails(opts.Email)

	case ChannelTempmailLa:
		return tempmailLaGetEmails(opts.Email)

	case ChannelTempMailIO:
		return tempMailIOGetEmails(opts.Email)

	case ChannelAwamail:
		if opts.Token == "" {
			return nil, fmt.Errorf("token is required for awamail channel")
		}
		return awamailGetEmails(opts.Token, opts.Email)

	case ChannelMailTm:
		if opts.Token == "" {
			return nil, fmt.Errorf("token is required for mail-tm channel")
		}
		return mailTmGetEmails(opts.Token, opts.Email)

	case ChannelDropmail:
		if opts.Token == "" {
			return nil, fmt.Errorf("token is required for dropmail channel")
		}
		return dropmailGetEmails(opts.Token, opts.Email)

	case ChannelGuerrillaMail:
		if opts.Token == "" {
			return nil, fmt.Errorf("token is required for guerrillamail channel")
		}
		return guerrillaMailGetEmails(opts.Token, opts.Email)

	case ChannelMaildrop:
		if opts.Token == "" {
			return nil, fmt.Errorf("token is required for maildrop channel")
		}
		return maildropGetEmails(opts.Token, opts.Email)

	default:
		return nil, fmt.Errorf("unknown channel: %s", opts.Channel)
	}
}

/*
 * Client 临时邮箱客户端
 * 封装了邮箱创建和邮件获取的完整流程，自动管理邮箱信息和认证令牌
 *
 * 示例:
 *   client := NewClient()
 *   info, _ := client.Generate(&GenerateEmailOptions{Channel: ChannelMailTm})
 *   result, _ := client.GetEmails()
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
func (c *Client) GetEmails() (*GetEmailsResult, error) {
	if c.emailInfo == nil {
		return nil, fmt.Errorf("no email generated. Call Generate() first")
	}

	return GetEmails(GetEmailsOptions{
		Channel: c.emailInfo.Channel,
		Email:   c.emailInfo.Email,
		Token:   c.emailInfo.Token,
	})
}

/* GetEmailInfo 获取当前缓存的邮箱信息，未调用 Generate() 时返回 nil */
func (c *Client) GetEmailInfo() *EmailInfo {
	return c.emailInfo
}
