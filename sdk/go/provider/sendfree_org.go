package provider

// sendfree.org：mailinator 官方姊妹域名，MX 指向 mail.mailinator.com。

// SendfreeOrgGenerate 创建 sendfree.org 临时邮箱
func SendfreeOrgGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "sendfree-org",
		Email:   local + "@sendfree.org",
	}, nil
}

// SendfreeOrgGetEmails 读取邮件（复用 mailinator public API）
func SendfreeOrgGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
