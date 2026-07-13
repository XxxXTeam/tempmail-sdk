package provider

// spam.lyceum-life.com.ru：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

// SpamLyceumLifeComRuGenerate 创建 spam.lyceum-life.com.ru 临时邮箱
func SpamLyceumLifeComRuGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "spam-lyceum-life-com-ru",
		Email:   local + "@spam.lyceum-life.com.ru",
	}, nil
}

// SpamLyceumLifeComRuGetEmails 读取邮件（复用 mailinator public API）
func SpamLyceumLifeComRuGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
