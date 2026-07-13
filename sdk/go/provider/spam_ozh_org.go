package provider

// spam.ozh.org：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

// SpamOzhOrgGenerate 创建 spam.ozh.org 临时邮箱
func SpamOzhOrgGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "spam-ozh-org",
		Email:   local + "@spam.ozh.org",
	}, nil
}

// SpamOzhOrgGetEmails 读取邮件（复用 mailinator public API）
func SpamOzhOrgGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
