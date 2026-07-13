package provider

// spam.jasonpearce.com：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

// SpamJasonpearceComGenerate 创建 spam.jasonpearce.com 临时邮箱
func SpamJasonpearceComGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "spam-jasonpearce-com",
		Email:   local + "@spam.jasonpearce.com",
	}, nil
}

// SpamJasonpearceComGetEmails 读取邮件（复用 mailinator public API）
func SpamJasonpearceComGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
