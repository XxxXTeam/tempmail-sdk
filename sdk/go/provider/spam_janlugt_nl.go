package provider

// spam.janlugt.nl：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

// SpamJanlugtNlGenerate 创建 spam.janlugt.nl 临时邮箱
func SpamJanlugtNlGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "spam-janlugt-nl",
		Email:   local + "@spam.janlugt.nl",
	}, nil
}

// SpamJanlugtNlGetEmails 读取邮件（复用 mailinator public API）
func SpamJanlugtNlGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
