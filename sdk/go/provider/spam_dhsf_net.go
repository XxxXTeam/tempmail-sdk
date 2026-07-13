package provider

// spam.dhsf.net：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

// SpamDhsfNetGenerate 创建 spam.dhsf.net 临时邮箱
func SpamDhsfNetGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "spam-dhsf-net",
		Email:   local + "@spam.dhsf.net",
	}, nil
}

// SpamDhsfNetGetEmails 读取邮件（复用 mailinator public API）
func SpamDhsfNetGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
