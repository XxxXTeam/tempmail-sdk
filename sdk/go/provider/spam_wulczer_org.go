package provider

// spam.wulczer.org：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

// SpamWulczerOrgGenerate 创建 spam.wulczer.org 临时邮箱
func SpamWulczerOrgGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "spam-wulczer-org",
		Email:   local + "@spam.wulczer.org",
	}, nil
}

// SpamWulczerOrgGetEmails 读取邮件（复用 mailinator public API）
func SpamWulczerOrgGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
