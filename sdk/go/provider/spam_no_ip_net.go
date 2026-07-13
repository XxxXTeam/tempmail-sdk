package provider

// spam.no-ip.net：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

// SpamNoIpNetGenerate 创建 spam.no-ip.net 临时邮箱
func SpamNoIpNetGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "spam-no-ip-net",
		Email:   local + "@spam.no-ip.net",
	}, nil
}

// SpamNoIpNetGetEmails 读取邮件（复用 mailinator public API）
func SpamNoIpNetGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
