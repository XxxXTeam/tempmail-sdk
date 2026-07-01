package tempemail

import (
	"fmt"
	"math/rand"

	prov "github.com/XxxXTeam/tempmail-sdk/sdk/go/provider"
)

/* 所有支持的渠道列表，用于随机选择和遍历 */
var allChannels = []Channel{
	ChannelTempmail,
	ChannelTempmailCn,
	ChannelTaEasy,
	Channel10minuteOne,
	ChannelLinshiyou,
	ChannelMffac,
	ChannelTempmailLol,
	ChannelChatgptOrgUk,
	ChannelTempMailIo,
	ChannelMailCx,
	ChannelCatchmail,
	ChannelCatchmailMailistry,
	ChannelCatchmailZeppost,
	ChannelMailforspam,
	ChannelMailforspamTempmailIo,
	ChannelMailforspamDisposable,
	ChannelTempmailc,
	ChannelMailnesia,
	ChannelThrowawaymail,
	ChannelInboxkitten,
	ChannelGetnada,
	ChannelMail123,
	ChannelOneSecMail,
	ChannelFakemail,
	ChannelOpeninbox,
	ChannelInboxes,
	ChannelUncorreotemporal,
	ChannelAwamail,
	ChannelMailTm,
	ChannelDropmail,
	ChannelGuerrillaMail,
	ChannelGuerrillamailCom,
	ChannelMaildrop,
	ChannelSmailPw,
	ChannelVip215,
	ChannelFakeLegal,
	ChannelMoakt,
	ChannelEmail10min,
	ChannelMjjCm,
	ChannelLinshiCo,
	ChannelHarakirimail,
	ChannelTempmailPlus,
	ChannelTempmailLolV2,
	ChannelSharklasers,
	ChannelSharklasersCom,
	ChannelGrrLa,
	ChannelGrrLaCom,
	ChannelGuerrillamailInfo,
	ChannelSpam4me,
	ChannelGuerrillamailNet,
	ChannelGuerrillamailOrg,
	ChannelGuerrillamailBlock,
	ChannelGuerrillamailComWww,
}

/*
 * ChannelInfo 渠道信息，包含渠道标识、显示名称和对应网站
 */
func fixedDomain(domain string) *string { return &domain }

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
	ChannelTempmail:              {Channel: ChannelTempmail, Name: "TempMail", Website: "tempmail.ing"},
	ChannelTempmailCn:            {Channel: ChannelTempmailCn, Name: "TempMail CN", Website: "tempmail.cn"},
	ChannelTaEasy:                {Channel: ChannelTaEasy, Name: "TA Easy", Website: "ta-easy.com"},
	Channel10minuteOne:           {Channel: Channel10minuteOne, Name: "10 Minute Email", Website: "10minutemail.one"},
	ChannelLinshiyou:             {Channel: ChannelLinshiyou, Name: "临时邮", Website: "linshiyou.com"},
	ChannelMffac:                 {Channel: ChannelMffac, Name: "MFFAC", Website: "mffac.com"},
	ChannelTempmailLol:           {Channel: ChannelTempmailLol, Name: "TempMail LOL", Website: "tempmail.lol"},
	ChannelChatgptOrgUk:          {Channel: ChannelChatgptOrgUk, Name: "ChatGPT Mail", Website: "mail.chatgpt.org.uk"},
	ChannelTempMailIo:            {Channel: ChannelTempMailIo, Name: "Temp-Mail.io", Website: "temp-mail.io"},
	ChannelMailCx:                {Channel: ChannelMailCx, Name: "Mail.cx", Website: "mail.cx"},
	ChannelCatchmail:             {Channel: ChannelCatchmail, Name: "Catchmail", Website: "catchmail.io"},
	ChannelCatchmailMailistry:    {Channel: ChannelCatchmailMailistry, Name: "Catchmail Mailistry", Website: "mailistry.com"},
	ChannelCatchmailZeppost:      {Channel: ChannelCatchmailZeppost, Name: "Catchmail Zeppost", Website: "zeppost.com"},
	ChannelMailforspam:           {Channel: ChannelMailforspam, Name: "MailForSpam", Website: "mailforspam.com"},
	ChannelMailforspamTempmailIo: {Channel: ChannelMailforspamTempmailIo, Name: "MailForSpam TempMail.io", Website: "tempmail.io"},
	ChannelMailforspamDisposable: {Channel: ChannelMailforspamDisposable, Name: "MailForSpam Disposable", Website: "disposable.email"},
	ChannelTempmailc:             {Channel: ChannelTempmailc, Name: "TempMailC", Website: "tempmailc.com"},
	ChannelMailnesia:             {Channel: ChannelMailnesia, Name: "Mailnesia", Website: "mailnesia.com"},
	ChannelThrowawaymail:         {Channel: ChannelThrowawaymail, Name: "ThrowawayMail", Website: "throwawaymail.app"},
	ChannelInboxkitten:           {Channel: ChannelInboxkitten, Name: "InboxKitten", Website: "inboxkitten.com"},
	ChannelGetnada:               {Channel: ChannelGetnada, Name: "GetNada", Website: "getnada.net"},
	ChannelMail123:               {Channel: ChannelMail123, Name: "Mail123", Website: "mail123.fr"},
	ChannelOneSecMail:            {Channel: ChannelOneSecMail, Name: "1SecMail", Website: "1sec-mail.com"},
	ChannelFakemail:              {Channel: ChannelFakemail, Name: "FakeMail", Website: "fakemail.net"},
	ChannelOpeninbox:             {Channel: ChannelOpeninbox, Name: "OpenInbox", Website: "openinbox.io"},
	ChannelInboxes:               {Channel: ChannelInboxes, Name: "Inboxes", Website: "inboxes.com"},
	ChannelUncorreotemporal:      {Channel: ChannelUncorreotemporal, Name: "UnCorreoTemporal", Website: "uncorreotemporal.com"},
	ChannelAwamail:               {Channel: ChannelAwamail, Name: "AwaMail", Website: "awamail.com"},
	ChannelMailTm:                {Channel: ChannelMailTm, Name: "Mail.tm", Website: "mail.tm"},
	ChannelDropmail:              {Channel: ChannelDropmail, Name: "DropMail", Website: "dropmail.me"},
	ChannelGuerrillaMail:         {Channel: ChannelGuerrillaMail, Name: "Guerrilla Mail", Website: "guerrillamail.com"},
	ChannelGuerrillamailCom:      {Channel: ChannelGuerrillamailCom, Name: "GuerrillaMail Root", Website: "guerrillamail.com"},
	ChannelMaildrop:              {Channel: ChannelMaildrop, Name: "Maildrop", Website: "maildrop.cx"},
	ChannelSmailPw:               {Channel: ChannelSmailPw, Name: "Smail.pw", Website: "smail.pw"},
	ChannelVip215:                {Channel: ChannelVip215, Name: "VIP 215", Website: "vip.215.im"},
	ChannelFakeLegal:             {Channel: ChannelFakeLegal, Name: "Fake Legal", Website: "fake.legal"},
	ChannelMoakt:                 {Channel: ChannelMoakt, Name: "Moakt", Website: "moakt.com"},
	ChannelEmail10min:            {Channel: ChannelEmail10min, Name: "Email10Min", Website: "email10min.com"},
	ChannelMjjCm:                 {Channel: ChannelMjjCm, Name: "MJJ Mail", Website: "mjj.cm"},
	ChannelLinshiCo:              {Channel: ChannelLinshiCo, Name: "临时Co", Website: "linshi.co"},
	ChannelHarakirimail:          {Channel: ChannelHarakirimail, Name: "HarakiriMail", Website: "harakirimail.com"},
	ChannelTempmailPlus:          {Channel: ChannelTempmailPlus, Name: "TempMail Plus", Website: "tempmail.plus"},
	ChannelTempmailLolV2:         {Channel: ChannelTempmailLolV2, Name: "TempMail LOL V2", Website: "tempmail.lol"},
	ChannelSharklasers:           {Channel: ChannelSharklasers, Name: "SharkLasers", Website: "sharklasers.com"},
	ChannelSharklasersCom:        {Channel: ChannelSharklasersCom, Name: "SharkLasers Root", Website: "sharklasers.com"},
	ChannelGrrLa:                 {Channel: ChannelGrrLa, Name: "Grr.la", Website: "grr.la"},
	ChannelGrrLaCom:              {Channel: ChannelGrrLaCom, Name: "Grr.la Root", Website: "grr.la"},
	ChannelGuerrillamailInfo:     {Channel: ChannelGuerrillamailInfo, Name: "GuerrillaMail Info", Website: "guerrillamail.info"},
	ChannelSpam4me:               {Channel: ChannelSpam4me, Name: "Spam4.me", Website: "spam4.me"},
	ChannelGuerrillamailNet:      {Channel: ChannelGuerrillamailNet, Name: "GuerrillaMail Net", Website: "guerrillamail.net"},
	ChannelGuerrillamailOrg:      {Channel: ChannelGuerrillamailOrg, Name: "GuerrillaMail Org", Website: "guerrillamail.org"},
	ChannelGuerrillamailBlock:    {Channel: ChannelGuerrillamailBlock, Name: "GuerrillaMailBlock", Website: "guerrillamailblock.com"},
	ChannelGuerrillamailComWww:   {Channel: ChannelGuerrillamailComWww, Name: "GuerrillaMail WWW", Website: "guerrillamail.com"},
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
 *   info, _ := GenerateEmail(&GenerateEmailOptions{Channel: ChannelMailTm})
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

	channelsTried := 0
	var lastErrMsg string
	for _, ch := range tryOrder {
		channelsTried++
		sdkLogger.Info("创建临时邮箱", "channel", string(ch))
		result, attempts, err := withRetryAndAttempts(func() (*EmailInfo, error) {
			return generateEmailOnce(ch, opts)
		}, opts.Retry)
		if err == nil && result != nil {
			sdkLogger.Info("邮箱创建成功", "channel", string(ch), "email", result.Email)
			reportTelemetry("generate_email", string(ch), true, attempts, channelsTried, "")
			return result, nil
		}
		errMsg := "unknown error"
		if err != nil {
			errMsg = err.Error()
			lastErrMsg = errMsg
		}
		sdkLogger.Warn("渠道不可用，尝试下一个", "channel", string(ch), "error", errMsg)
	}

	sdkLogger.Error("所有渠道均不可用，创建邮箱失败")
	reportTelemetry("generate_email", "", false, 0, channelsTried, lastErrMsg)
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
		return fromMailbox(prov.TempmailGenerate(duration))

	case ChannelTempmailCn:
		return fromMailbox(prov.TempmailCNGenerate(opts.Domain))
	case ChannelTaEasy:
		return fromMailbox(prov.TaEasyGenerate())

	case Channel10minuteOne:
		return fromMailbox(prov.TenminuteOneGenerate(opts.Domain))

	case ChannelLinshiyou:
		return fromMailbox(prov.LinshiyouGenerate())

	case ChannelMffac:
		return fromMailbox(prov.MffacGenerate())

	case ChannelTempmailLol:
		return fromMailbox(prov.TempmailLolGenerate(opts.Domain))

	case ChannelChatgptOrgUk:
		return fromMailbox(prov.ChatgptOrgUkGenerate())

	case ChannelTempMailIo:
		return fromMailbox(prov.TempMailIoGenerate())

	case ChannelMailCx:
		return fromMailbox(prov.MailCxGenerate(opts.Domain))

	case ChannelCatchmail:
		return fromMailbox(prov.CatchmailGenerate(opts.Domain))

	case ChannelCatchmailMailistry:
		return fromMailbox(prov.CatchmailGenerate(fixedDomain("mailistry.com"), string(ChannelCatchmailMailistry)))

	case ChannelCatchmailZeppost:
		return fromMailbox(prov.CatchmailGenerate(fixedDomain("zeppost.com"), string(ChannelCatchmailZeppost)))

	case ChannelMailforspam:
		return fromMailbox(prov.MailforspamGenerate(opts.Domain))

	case ChannelMailforspamTempmailIo:
		return fromMailbox(prov.MailforspamGenerate(fixedDomain("tempmail.io"), string(ChannelMailforspamTempmailIo)))

	case ChannelMailforspamDisposable:
		return fromMailbox(prov.MailforspamGenerate(fixedDomain("disposable.email"), string(ChannelMailforspamDisposable)))

	case ChannelTempmailc:
		return fromMailbox(prov.TempmailcGenerate())

	case ChannelMailnesia:
		return fromMailbox(prov.MailnesiaGenerate())

	case ChannelThrowawaymail:
		return fromMailbox(prov.ThrowawaymailGenerate())

	case ChannelInboxkitten:
		return fromMailbox(prov.InboxkittenGenerate())

	case ChannelGetnada:
		return fromMailbox(prov.GetnadaGenerate(opts.Domain))

	case ChannelMail123:
		return fromMailbox(prov.Mail123Generate())

	case ChannelOneSecMail:
		return fromMailbox(prov.OneSecMailGenerate())

	case ChannelFakemail:
		return fromMailbox(prov.FakemailGenerate())

	case ChannelOpeninbox:
		return fromMailbox(prov.OpeninboxGenerate())

	case ChannelInboxes:
		return fromMailbox(prov.InboxesGenerate(opts.Domain))

	case ChannelUncorreotemporal:
		return fromMailbox(prov.UncorreotemporalGenerate())

	case ChannelAwamail:
		return fromMailbox(prov.AwamailGenerate())

	case ChannelMailTm:
		return fromMailbox(prov.MailTmGenerate())

	case ChannelDropmail:
		return fromMailbox(prov.DropmailGenerate())

	case ChannelGuerrillaMail:
		return fromMailbox(prov.GuerrillaMailGenerate())

	case ChannelGuerrillamailCom:
		return fromMailbox(prov.GuerrillamailMirrorGenerate("guerrillamail-com", "https://guerrillamail.com/ajax.php"))

	case ChannelMaildrop:
		return fromMailbox(prov.MaildropGenerate(opts.Domain))

	case ChannelSmailPw:
		return fromMailbox(prov.SmailPwGenerate())
	case ChannelVip215:
		return fromMailbox(prov.MailVip215Generate())
	case ChannelFakeLegal:
		return fromMailbox(prov.FakeLegalGenerate(opts.Domain))

	case ChannelMoakt:
		return fromMailbox(prov.MoaktGenerate(opts.Domain))
	case ChannelEmail10min:
		return fromMailbox(prov.Email10minGenerate())

	case ChannelMjjCm:
		return fromMailbox(prov.MjjCmGenerate())
	case ChannelLinshiCo:
		return fromMailbox(prov.LinshiCoGenerate())

	case ChannelHarakirimail:
		return fromMailbox(prov.HarakirimailGenerate())

	case ChannelTempmailPlus:
		return fromMailbox(prov.TempmailPlusGenerate(opts.Domain))

	case ChannelTempmailLolV2:
		return fromMailbox(prov.TempmailLolV2Generate())

	case ChannelSharklasers:
		return fromMailbox(prov.GuerrillamailMirrorGenerate("sharklasers", "https://www.sharklasers.com/ajax.php"))

	case ChannelSharklasersCom:
		return fromMailbox(prov.GuerrillamailMirrorGenerate("sharklasers-com", "https://sharklasers.com/ajax.php"))

	case ChannelGrrLa:
		return fromMailbox(prov.GuerrillamailMirrorGenerate("grr-la", "https://www.grr.la/ajax.php"))

	case ChannelGrrLaCom:
		return fromMailbox(prov.GuerrillamailMirrorGenerate("grr-la-com", "https://grr.la/ajax.php"))

	case ChannelGuerrillamailInfo:
		return fromMailbox(prov.GuerrillamailMirrorGenerate("guerrillamail-info", "https://www.guerrillamail.info/ajax.php"))

	case ChannelSpam4me:
		return fromMailbox(prov.GuerrillamailMirrorGenerate("spam4me", "https://www.spam4.me/ajax.php"))

	case ChannelGuerrillamailNet:
		return fromMailbox(prov.GuerrillamailMirrorGenerate("guerrillamail-net", "https://www.guerrillamail.net/ajax.php"))

	case ChannelGuerrillamailOrg:
		return fromMailbox(prov.GuerrillamailMirrorGenerate("guerrillamail-org", "https://www.guerrillamail.org/ajax.php"))

	case ChannelGuerrillamailBlock:
		return fromMailbox(prov.GuerrillamailMirrorGenerate("guerrillamailblock", "https://www.guerrillamailblock.com/ajax.php"))

	case ChannelGuerrillamailComWww:
		return fromMailbox(prov.GuerrillamailMirrorGenerate("guerrillamail-com-www", "https://www.guerrillamail.com/ajax.php"))

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
		reportTelemetry("get_emails", "", false, 0, 0, "EmailInfo is required, call GenerateEmail() first")
		return nil, fmt.Errorf("EmailInfo is required, call GenerateEmail() first")
	}
	if info.Channel == "" {
		reportTelemetry("get_emails", "", false, 0, 0, "channel is required")
		return nil, fmt.Errorf("channel is required")
	}
	if info.Email == "" && info.Channel != ChannelTempmailLol {
		reportTelemetry("get_emails", string(info.Channel), false, 0, 0, "email is required")
		return nil, fmt.Errorf("email is required")
	}

	var retry *RetryOptions
	if opts != nil {
		retry = opts.Retry
	}

	sdkLogger.Debug("获取邮件", "channel", string(info.Channel), "email", info.Email)
	emails, attempts, err := withRetryAndAttempts(func() ([]Email, error) {
		return getEmailsOnce(info.Channel, info.Email, info.token)
	}, retry)

	if err != nil {
		reportTelemetry("get_emails", string(info.Channel), false, attempts, 0, err.Error())
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
	reportTelemetry("get_emails", string(info.Channel), true, attempts, 0, "")

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
		return normEmailsResult(prov.TempmailGetEmails(email))

	case ChannelTempmailCn:
		return normEmailsResult(prov.TempmailCNGetEmails(email))
	case ChannelTaEasy:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for ta-easy channel")
		}
		return normEmailsResult(prov.TaEasyGetEmails(email, token))

	case Channel10minuteOne:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for 10minute-one channel")
		}
		return normEmailsResult(prov.TenminuteOneGetEmails(email, token))

	case ChannelLinshiyou:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for linshiyou channel")
		}
		return normEmailsResult(prov.LinshiyouGetEmails(token, email))

	case ChannelMffac:
		return normEmailsResult(prov.MffacGetEmails(email, token))

	case ChannelTempmailLol:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for tempmail-lol channel")
		}
		return normEmailsResult(prov.TempmailLolGetEmails(token, email))

	case ChannelChatgptOrgUk:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for chatgpt-org-uk channel")
		}
		return normEmailsResult(prov.ChatgptOrgUkGetEmails(email, token))

	case ChannelTempMailIo:
		return normEmailsResult(prov.TempMailIoGetEmails(email))

	case ChannelMailCx:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for mail-cx channel")
		}
		return normEmailsResult(prov.MailCxGetEmails(token, email))

	case ChannelCatchmail, ChannelCatchmailMailistry, ChannelCatchmailZeppost:
		return normEmailsResult(prov.CatchmailGetEmails(email))

	case ChannelMailforspam, ChannelMailforspamTempmailIo, ChannelMailforspamDisposable:
		return normEmailsResult(prov.MailforspamGetEmails(email))

	case ChannelTempmailc:
		return normEmailsResult(prov.TempmailcGetEmails(email))

	case ChannelMailnesia:
		return normEmailsResult(prov.MailnesiaGetEmails(email))

	case ChannelThrowawaymail:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for throwawaymail channel")
		}
		return normEmailsResult(prov.ThrowawaymailGetEmails(token, email))

	case ChannelInboxkitten:
		return normEmailsResult(prov.InboxkittenGetEmails(email))

	case ChannelGetnada:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for getnada channel")
		}
		return normEmailsResult(prov.GetnadaGetEmails(token, email))

	case ChannelMail123:
		return normEmailsResult(prov.Mail123GetEmails(email))

	case ChannelOneSecMail:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for 1sec-mail channel")
		}
		return normEmailsResult(prov.OneSecMailGetEmails(token, email))

	case ChannelFakemail:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for fakemail channel")
		}
		return normEmailsResult(prov.FakemailGetEmails(token, email))

	case ChannelOpeninbox:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for openinbox channel")
		}
		return normEmailsResult(prov.OpeninboxGetEmails(token, email))

	case ChannelInboxes:
		return normEmailsResult(prov.InboxesGetEmails(email))

	case ChannelUncorreotemporal:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for uncorreotemporal channel")
		}
		return normEmailsResult(prov.UncorreotemporalGetEmails(token, email))

	case ChannelAwamail:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for awamail channel")
		}
		return normEmailsResult(prov.AwamailGetEmails(token, email))

	case ChannelMailTm:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for mail-tm channel")
		}
		return normEmailsResult(prov.MailTmGetEmails(token, email))

	case ChannelDropmail:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for dropmail channel")
		}
		return normEmailsResult(prov.DropmailGetEmails(token, email))

	case ChannelGuerrillaMail:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for guerrillamail channel")
		}
		return normEmailsResult(prov.GuerrillaMailGetEmails(token, email))

	case ChannelGuerrillamailCom:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for guerrillamail-com channel")
		}
		return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://guerrillamail.com/ajax.php", token, email))

	case ChannelMaildrop:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for maildrop channel")
		}
		return normEmailsResult(prov.MaildropGetEmails(token, email))

	case ChannelSmailPw:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for smail-pw channel")
		}
		return normEmailsResult(prov.SmailPwGetEmails(token, email))
	case ChannelVip215:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for vip-215 channel")
		}
		return normEmailsResult(prov.MailVip215GetEmails(token, email))
	case ChannelFakeLegal:
		return normEmailsResult(prov.FakeLegalGetEmails(email))

	case ChannelMoakt:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for moakt channel")
		}
		return normEmailsResult(prov.MoaktGetEmails(email, token))
	case ChannelEmail10min:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for email10min channel")
		}
		return normEmailsResult(prov.Email10minGetEmails(email, token))

	case ChannelMjjCm:
		return normEmailsResult(prov.MjjCmGetEmails(email))
	case ChannelLinshiCo:
		return normEmailsResult(prov.LinshiCoGetEmails(email))

	case ChannelHarakirimail:
		return normEmailsResult(prov.HarakirimailGetEmails(email))

	case ChannelTempmailPlus:
		return normEmailsResult(prov.TempmailPlusGetEmails(email))

	case ChannelTempmailLolV2:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for tempmail-lol-v2 channel")
		}
		return normEmailsResult(prov.TempmailLolV2GetEmails(token, email))

	case ChannelSharklasers:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for sharklasers channel")
		}
		return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://www.sharklasers.com/ajax.php", token, email))

	case ChannelSharklasersCom:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for sharklasers-com channel")
		}
		return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://sharklasers.com/ajax.php", token, email))

	case ChannelGrrLa:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for grr-la channel")
		}
		return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://www.grr.la/ajax.php", token, email))

	case ChannelGrrLaCom:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for grr-la-com channel")
		}
		return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://grr.la/ajax.php", token, email))

	case ChannelGuerrillamailInfo:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for guerrillamail-info channel")
		}
		return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://www.guerrillamail.info/ajax.php", token, email))

	case ChannelSpam4me:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for spam4me channel")
		}
		return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://www.spam4.me/ajax.php", token, email))

	case ChannelGuerrillamailNet:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for guerrillamail-net channel")
		}
		return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://www.guerrillamail.net/ajax.php", token, email))

	case ChannelGuerrillamailOrg:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for guerrillamail-org channel")
		}
		return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://www.guerrillamail.org/ajax.php", token, email))

	case ChannelGuerrillamailBlock:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for guerrillamailblock channel")
		}
		return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://www.guerrillamailblock.com/ajax.php", token, email))

	case ChannelGuerrillamailComWww:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for guerrillamail-com-www channel")
		}
		return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://www.guerrillamail.com/ajax.php", token, email))

	default:
		return nil, fmt.Errorf("unsupported channel: %s", channel)
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
		reportTelemetry("get_emails", "", false, 0, 0, "no email generated. Call Generate() first")
		return nil, fmt.Errorf("no email generated. Call Generate() first")
	}

	return GetEmails(c.emailInfo, opts)
}

/* GetEmailInfo 获取当前缓存的邮箱信息，未调用 Generate() 时返回 nil */
func (c *Client) GetEmailInfo() *EmailInfo {
	return c.emailInfo
}
