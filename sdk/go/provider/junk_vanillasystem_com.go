package provider

// junk.vanillasystem.com：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

// JunkVanillasystemComGenerate 创建 junk.vanillasystem.com 临时邮箱
func JunkVanillasystemComGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "junk-vanillasystem-com",
		Email:   local + "@junk.vanillasystem.com",
	}, nil
}

// JunkVanillasystemComGetEmails 读取邮件（复用 mailinator public API）
func JunkVanillasystemComGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
