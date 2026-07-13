package provider

// chammy.info：mailinator 官方姊妹域名，MX 指向 mail.mailinator.com。

// ChammyInfoGenerate 创建 chammy.info 临时邮箱
func ChammyInfoGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "chammy-info",
		Email:   local + "@chammy.info",
	}, nil
}

// ChammyInfoGetEmails 读取 chammy.info 邮件（复用 mailinator public API）
func ChammyInfoGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
