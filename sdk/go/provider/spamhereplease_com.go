package provider

// spamhereplease.com：mailinator 官方姊妹域名，MX 指向 mail.mailinator.com。

// SpamherepleaseComGenerate 创建 spamhereplease.com 临时邮箱
func SpamherepleaseComGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "spamhereplease-com",
		Email:   local + "@spamhereplease.com",
	}, nil
}

// SpamherepleaseComGetEmails 读取邮件（复用 mailinator public API）
func SpamherepleaseComGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
