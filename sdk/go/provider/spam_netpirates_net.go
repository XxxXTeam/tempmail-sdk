package provider

// spam.netpirates.net：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

// SpamNetpiratesNetGenerate 创建 spam.netpirates.net 临时邮箱
func SpamNetpiratesNetGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "spam-netpirates-net",
		Email:   local + "@spam.netpirates.net",
	}, nil
}

// SpamNetpiratesNetGetEmails 读取邮件（复用 mailinator public API）
func SpamNetpiratesNetGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
