package provider

// fish.skytale.net：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

// FishSkytaleNetGenerate 创建 fish.skytale.net 临时邮箱
func FishSkytaleNetGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "fish-skytale-net",
		Email:   local + "@fish.skytale.net",
	}, nil
}

// FishSkytaleNetGetEmails 读取邮件（复用 mailinator public API）
func FishSkytaleNetGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
