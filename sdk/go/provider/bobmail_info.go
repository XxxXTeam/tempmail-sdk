package provider

// bobmail.info：mailinator 官方姊妹域名，MX 指向 mail.mailinator.com。

// BobmailInfoGenerate 创建 bobmail.info 临时邮箱
func BobmailInfoGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "bobmail-info",
		Email:   local + "@bobmail.info",
	}, nil
}

// BobmailInfoGetEmails 读取 bobmail.info 邮件（复用 mailinator public API）
func BobmailInfoGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
