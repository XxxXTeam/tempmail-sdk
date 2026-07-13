package provider

// notmailinator.com：mailinator 官方姊妹域名，MX 指向 mail.mailinator.com。

// NotmailinatorComGenerate 创建 notmailinator.com 临时邮箱
func NotmailinatorComGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "notmailinator-com",
		Email:   local + "@notmailinator.com",
	}, nil
}

// NotmailinatorComGetEmails 读取邮件（复用 mailinator public API）
func NotmailinatorComGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
