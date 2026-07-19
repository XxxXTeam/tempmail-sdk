package tempemail

import "strings"

// channelDomains 维护渠道到已知支持域名的静态映射
var channelDomains = map[Channel][]string{
	// Gmail 相关
	ChannelEmailnator:  {"gmail.com", "googlemail.com"},
	ChannelTempgmailer: {"gmail.com"},

	// 多域名渠道
	ChannelCatchmail:             {"catchmail.io", "mailistry.com", "zeppost.com"},
	ChannelCatchmailMailistry:    {"mailistry.com"},
	ChannelCatchmailZeppost:      {"zeppost.com"},
	ChannelMailforspam:           {"mailforspam.com", "tempmail.io", "disposable.email"},
	ChannelMailforspamTempmailIo: {"tempmail.io"},
	ChannelMailforspamDisposable: {"disposable.email"},
	ChannelTempmail365:           {"fengyou.cc", "shop345.com", "nutemail.com", "qvrf.cn"},
	ChannelTempinbox:             {"tempinbox.xyz", "thepiratebay.cloud", "cryptoblad.nl"},
	Channel10minuteOne:           {"xghff.com", "oqqaj.com", "psovv.com", "dbwot.com", "ygwpr.com", "imxwe.com"},
	ChannelXghffCom:              {"xghff.com"},
	ChannelOqqajCom:              {"oqqaj.com"},
	ChannelPsovvCom:              {"psovv.com"},
	ChannelDbwotCom:              {"dbwot.com"},
	ChannelYgwprCom:              {"ygwpr.com"},
	ChannelImxweCom:              {"imxwe.com"},
	ChannelApihz:                 {"apimail.email", "apimail.vip"},
	Channel24mailChacuo:          {"chacuo.net", "027168.com"},

	// mailinator 系列
	ChannelMailinator:              {"mailinator.com"},
	ChannelSogetthisCom:            {"sogetthis.com"},
	ChannelBobmailInfo:             {"bobmail.info"},
	ChannelSuremailInfo:            {"suremail.info"},
	ChannelBinkmailCom:             {"binkmail.com"},
	ChannelVeryrealemailCom:        {"veryrealemail.com"},
	ChannelChammyInfo:              {"chammy.info"},
	ChannelThisisnotmyrealemailCom: {"thisisnotmyrealemail.com"},
	ChannelNotmailinatorCom:        {"notmailinator.com"},
	ChannelSpamherepleaseCom:       {"spamhereplease.com"},
	ChannelSendspamhereCom:         {"sendspamhere.com"},
	ChannelSendfreeOrg:             {"sendfree.org"},

	// 其他单域名
	ChannelMailnesia:    {"mailnesia.com"},
	ChannelHarakirimail: {"harakirimail.com"},
	ChannelByom:         {"byom.de"},
	ChannelEyepaste:     {"eyepaste.com"},
	ChannelMailcatch:    {"mailcatch.com"},
	ChannelMaildropCc:   {"maildrop.cc"},
	ChannelSmailPw:      {"smail.pw"},
	ChannelInboxkitten:  {"inboxkitten.com"},
	ChannelMffac:        {"mffac.com"},
	ChannelNimail:       {"nimail.cn"},
	ChannelMailgolem:    {"mailgolem.com"},
	ChannelMohmal:       {"emailinbo.live"},

	// tempmail-plus 系列
	ChannelTempmailPlus: {"mailto.plus"},
	ChannelFexpostCom:   {"fexpost.com"},
	ChannelFexboxOrg:    {"fexbox.org"},
	ChannelMailboxInUa:  {"mailbox.in.ua"},
	ChannelRoverInfo:    {"rover.info"},
	ChannelChitthiIn:    {"chitthi.in"},
	ChannelFextempCom:   {"fextemp.com"},
	ChannelAnyPink:      {"any.pink"},
	ChannelMerepostCom:  {"merepost.com"},

	// getnada 系列
	ChannelGetnada:      {"getnada.com", "getnada.email", "getnada.net"},
	ChannelGetnadaCom:   {"getnada.com"},
	ChannelGetnadaEmail: {"getnada.email"},
	ChannelGetnadaNet:   {"getnada.net"},

	// disposablemail
	ChannelDisposablemailApp: {"disposablemail.dev", "mailmehere.cc"},

	// minuteinbox
	ChannelMinuteinbox: {"minafter.com"},

	// xkx.me
	ChannelXkxMe: {"xkx.me"},
}

// dynamicDomainChannels 是域名通过 API 动态获取的渠道
// 在域名筛选时默认保留这些渠道（无法预判其支持的域名）
var dynamicDomainChannels = map[Channel]bool{
	ChannelMailTm:        true,
	ChannelDuckmail:      true,
	ChannelMailTd:        true,
	ChannelChatgptOrgUk:  true,
	ChannelTemporam:      true,
	ChannelGetnada:       true,
	ChannelNeighbours:    true,
	ChannelNeighboursSh:  true,
	ChannelInboxes:       true,
	ChannelMailmomy:      true,
	ChannelFreecustom:    true,
	ChannelMailSunls:     true,
	ChannelAnonymmail:    true,
	ChannelFakeLegal:     true,
	ChannelMaildrop:      true,
	ChannelMailCx:        true,
	ChannelMoakt:         true,
	ChannelTempmailLol:   true,
	ChannelSmailpro:      true,
	ChannelTempmail:      true,
	ChannelOneSecMail:    true,
	ChannelGuerrillaMail: true,
	ChannelDropmail:      true,
	ChannelFmail:         true,
	ChannelOckito:        true,
	ChannelTempMailOrg:   true,
}

// filterChannelsByDomain 根据目标域名过滤渠道列表
// targetDomains 为用户指定的域名列表
// 返回过滤后的渠道列表；如果 targetDomains 为空则返回原列表
func filterChannelsByDomain(channels []Channel, targetDomains []string) []Channel {
	if len(targetDomains) == 0 {
		return channels
	}

	var filtered []Channel
	for _, ch := range channels {
		// 动态域名渠道默认保留
		if dynamicDomainChannels[ch] {
			filtered = append(filtered, ch)
			continue
		}
		// 检查该渠道是否支持目标域名
		domains, ok := channelDomains[ch]
		if !ok {
			continue
		}
		if matchesDomain(domains, targetDomains) {
			filtered = append(filtered, ch)
		}
	}

	// 如果过滤后为空，回退到原列表
	if len(filtered) == 0 {
		return channels
	}
	return filtered
}

// matchesDomain 检查渠道支持的域名列表是否包含任一目标域名
func matchesDomain(chDomains []string, targetDomains []string) bool {
	for _, cd := range chDomains {
		for _, td := range targetDomains {
			// 精确匹配或子域名匹配
			if cd == td || strings.HasSuffix(cd, "."+td) {
				return true
			}
		}
	}
	return false
}
