package provider

// spam.lucatnt.com：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

// SpamLucatntComGenerate 创建 spam.lucatnt.com 临时邮箱
func SpamLucatntComGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "spam-lucatnt-com",
		Email:   local + "@spam.lucatnt.com",
	}, nil
}

// SpamLucatntComGetEmails 读取邮件（复用 mailinator public API）
func SpamLucatntComGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
