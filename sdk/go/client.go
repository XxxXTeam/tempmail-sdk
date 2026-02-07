package tempemail

import (
	"fmt"
	"math/rand"
	"time"
)

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
}

type ChannelInfo struct {
	Channel Channel
	Name    string
	Website string
}

var channelInfoMap = map[Channel]ChannelInfo{
	ChannelTempmail:     {Channel: ChannelTempmail, Name: "TempMail", Website: "tempmail.ing"},
	ChannelLinshiEmail:  {Channel: ChannelLinshiEmail, Name: "临时邮箱", Website: "linshi-email.com"},
	ChannelTempmailLol:  {Channel: ChannelTempmailLol, Name: "TempMail LOL", Website: "tempmail.lol"},
	ChannelChatgptOrgUk: {Channel: ChannelChatgptOrgUk, Name: "ChatGPT Mail", Website: "mail.chatgpt.org.uk"},
	ChannelTempmailLa:   {Channel: ChannelTempmailLa, Name: "TempMail LA", Website: "tempmail.la"},
	ChannelTempMailIO:   {Channel: ChannelTempMailIO, Name: "Temp Mail IO", Website: "temp-mail.io"},
	ChannelAwamail:      {Channel: ChannelAwamail, Name: "AwaMail", Website: "awamail.com"},
	ChannelMailTm:       {Channel: ChannelMailTm, Name: "Mail.tm", Website: "mail.tm"},
	ChannelDropmail:     {Channel: ChannelDropmail, Name: "DropMail", Website: "dropmail.me"},
}

func init() {
	rand.Seed(time.Now().UnixNano())
}

// ListChannels 获取所有支持的渠道列表
func ListChannels() []ChannelInfo {
	result := make([]ChannelInfo, len(allChannels))
	for i, ch := range allChannels {
		result[i] = channelInfoMap[ch]
	}
	return result
}

// GetChannelInfo 获取指定渠道信息
func GetChannelInfo(channel Channel) (ChannelInfo, bool) {
	info, ok := channelInfoMap[channel]
	return info, ok
}

func GenerateEmail(opts *GenerateEmailOptions) (*EmailInfo, error) {
	if opts == nil {
		opts = &GenerateEmailOptions{}
	}

	channel := opts.Channel
	if channel == "" {
		channel = allChannels[rand.Intn(len(allChannels))]
	}

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

	default:
		return nil, fmt.Errorf("unknown channel: %s", channel)
	}
}

func GetEmails(opts GetEmailsOptions) (*GetEmailsResult, error) {
	if opts.Channel == "" {
		return nil, fmt.Errorf("channel is required")
	}
	if opts.Email == "" && opts.Channel != ChannelTempmailLol {
		return nil, fmt.Errorf("email is required")
	}

	var emails []Email
	var err error

	switch opts.Channel {
	case ChannelTempmail:
		emails, err = tempmailGetEmails(opts.Email)

	case ChannelLinshiEmail:
		emails, err = linshiEmailGetEmails(opts.Email)

	case ChannelTempmailLol:
		if opts.Token == "" {
			return nil, fmt.Errorf("token is required for tempmail-lol channel")
		}
		emails, err = tempmailLolGetEmails(opts.Token, opts.Email)

	case ChannelChatgptOrgUk:
		emails, err = chatgptOrgUkGetEmails(opts.Email)

	case ChannelTempmailLa:
		emails, err = tempmailLaGetEmails(opts.Email)

	case ChannelTempMailIO:
		emails, err = tempMailIOGetEmails(opts.Email)

	case ChannelAwamail:
		if opts.Token == "" {
			return nil, fmt.Errorf("token is required for awamail channel")
		}
		emails, err = awamailGetEmails(opts.Token, opts.Email)

	case ChannelMailTm:
		if opts.Token == "" {
			return nil, fmt.Errorf("token is required for mail-tm channel")
		}
		emails, err = mailTmGetEmails(opts.Token, opts.Email)

	case ChannelDropmail:
		if opts.Token == "" {
			return nil, fmt.Errorf("token is required for dropmail channel")
		}
		emails, err = dropmailGetEmails(opts.Token, opts.Email)

	default:
		return nil, fmt.Errorf("unknown channel: %s", opts.Channel)
	}

	if err != nil {
		return nil, err
	}

	return &GetEmailsResult{
		Channel: opts.Channel,
		Email:   opts.Email,
		Emails:  emails,
		Success: true,
	}, nil
}

type Client struct {
	emailInfo *EmailInfo
}

func NewClient() *Client {
	return &Client{}
}

func (c *Client) Generate(opts *GenerateEmailOptions) (*EmailInfo, error) {
	info, err := GenerateEmail(opts)
	if err != nil {
		return nil, err
	}
	c.emailInfo = info
	return info, nil
}

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

func (c *Client) GetEmailInfo() *EmailInfo {
	return c.emailInfo
}
