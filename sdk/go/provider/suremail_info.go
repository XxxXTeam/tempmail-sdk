package provider

// suremail.info：mailinator 官方姊妹域名，MX 指向 mail.mailinator.com。

// SuremailInfoGenerate 创建 suremail.info 临时邮箱
func SuremailInfoGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "suremail-info",
		Email:   local + "@suremail.info",
	}, nil
}

// SuremailInfoGetEmails 读取 suremail.info 邮件（复用 mailinator public API）
func SuremailInfoGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
