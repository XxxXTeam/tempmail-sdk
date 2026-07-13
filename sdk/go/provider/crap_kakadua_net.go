package provider

// crap.kakadua.net：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

// CrapKakaduaNetGenerate 创建 crap.kakadua.net 临时邮箱
func CrapKakaduaNetGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "crap-kakadua-net",
		Email:   local + "@crap.kakadua.net",
	}, nil
}

// CrapKakaduaNetGetEmails 读取邮件（复用 mailinator public API）
func CrapKakaduaNetGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
