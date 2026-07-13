package provider

// spam.pyphus.org：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

// SpamPyphusOrgGenerate 创建 spam.pyphus.org 临时邮箱
func SpamPyphusOrgGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "spam-pyphus-org",
		Email:   local + "@spam.pyphus.org",
	}, nil
}

// SpamPyphusOrgGetEmails 读取邮件（复用 mailinator public API）
func SpamPyphusOrgGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
