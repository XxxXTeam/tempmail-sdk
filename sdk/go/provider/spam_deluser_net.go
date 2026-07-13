package provider

// spam.deluser.net：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

// SpamDeluserNetGenerate 创建 spam.deluser.net 临时邮箱
func SpamDeluserNetGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "spam-deluser-net",
		Email:   local + "@spam.deluser.net",
	}, nil
}

// SpamDeluserNetGetEmails 读取邮件（复用 mailinator public API）
func SpamDeluserNetGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
