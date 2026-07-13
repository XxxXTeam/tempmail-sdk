package provider

// junk.noplay.org：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

// JunkNoplayOrgGenerate 创建 junk.noplay.org 临时邮箱
func JunkNoplayOrgGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "junk-noplay-org",
		Email:   local + "@junk.noplay.org",
	}, nil
}

// JunkNoplayOrgGetEmails 读取邮件（复用 mailinator public API）
func JunkNoplayOrgGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
