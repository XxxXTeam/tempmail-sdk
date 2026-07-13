package provider

// spam.coroiu.com：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

// SpamCoroiuComGenerate 创建 spam.coroiu.com 临时邮箱
func SpamCoroiuComGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "spam-coroiu-com",
		Email:   local + "@spam.coroiu.com",
	}, nil
}

// SpamCoroiuComGetEmails 读取邮件（复用 mailinator public API）
func SpamCoroiuComGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
