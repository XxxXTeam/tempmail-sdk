package provider

// junk.beats.org：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

// JunkBeatsOrgGenerate 创建 junk.beats.org 临时邮箱
func JunkBeatsOrgGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "junk-beats-org",
		Email:   local + "@junk.beats.org",
	}, nil
}

// JunkBeatsOrgGetEmails 读取邮件（复用 mailinator public API）
func JunkBeatsOrgGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
