package provider

// binkmail.com：mailinator 官方姊妹域名，MX 指向 mail.mailinator.com。

// BinkmailComGenerate 创建 binkmail.com 临时邮箱
func BinkmailComGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "binkmail-com",
		Email:   local + "@binkmail.com",
	}, nil
}

// BinkmailComGetEmails 读取 binkmail.com 邮件（复用 mailinator public API）
func BinkmailComGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
