package provider

// veryrealemail.com：mailinator 官方姊妹域名，MX 指向 mail.mailinator.com。

// VeryrealemailComGenerate 创建 veryrealemail.com 临时邮箱
func VeryrealemailComGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "veryrealemail-com",
		Email:   local + "@veryrealemail.com",
	}, nil
}

// VeryrealemailComGetEmails 读取 veryrealemail.com 邮件（复用 mailinator public API）
func VeryrealemailComGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
