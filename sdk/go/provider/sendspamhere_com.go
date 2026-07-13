package provider

// sendspamhere.com：mailinator 官方姊妹域名，MX 指向 mail.mailinator.com。

// SendspamhereComGenerate 创建 sendspamhere.com 临时邮箱
func SendspamhereComGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "sendspamhere-com",
		Email:   local + "@sendspamhere.com",
	}, nil
}

// SendspamhereComGetEmails 读取邮件（复用 mailinator public API）
func SendspamhereComGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
