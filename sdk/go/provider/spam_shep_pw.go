package provider

// spam.shep.pw：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

// SpamShepPwGenerate 创建 spam.shep.pw 临时邮箱
func SpamShepPwGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "spam-shep-pw",
		Email:   local + "@spam.shep.pw",
	}, nil
}

// SpamShepPwGetEmails 读取邮件（复用 mailinator public API）
func SpamShepPwGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
