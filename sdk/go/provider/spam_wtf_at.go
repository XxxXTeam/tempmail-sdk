package provider

// spam.wtf.at：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

// SpamWtfAtGenerate 创建 spam.wtf.at 临时邮箱
func SpamWtfAtGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "spam-wtf-at",
		Email:   local + "@spam.wtf.at",
	}, nil
}

// SpamWtfAtGetEmails 读取邮件（复用 mailinator public API）
func SpamWtfAtGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
