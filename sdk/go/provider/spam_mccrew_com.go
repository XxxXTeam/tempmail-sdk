package provider

// spam.mccrew.com：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

// SpamMccrewComGenerate 创建 spam.mccrew.com 临时邮箱
func SpamMccrewComGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "spam-mccrew-com",
		Email:   local + "@spam.mccrew.com",
	}, nil
}

// SpamMccrewComGetEmails 读取邮件（复用 mailinator public API）
func SpamMccrewComGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
