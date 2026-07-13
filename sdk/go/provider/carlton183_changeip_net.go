package provider

// 183carlton.changeip.net：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

func Carlton183ChangeipNetGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "carlton183-changeip-net",
		Email:   local + "@183carlton.changeip.net",
	}, nil
}

func Carlton183ChangeipNetGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
