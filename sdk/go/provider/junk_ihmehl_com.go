package provider

// junk.ihmehl.com：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

// JunkIhmehlComGenerate 创建 junk.ihmehl.com 临时邮箱
func JunkIhmehlComGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "junk-ihmehl-com",
		Email:   local + "@junk.ihmehl.com",
	}, nil
}

// JunkIhmehlComGetEmails 读取邮件（复用 mailinator public API）
func JunkIhmehlComGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
