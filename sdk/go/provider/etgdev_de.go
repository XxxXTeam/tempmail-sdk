package provider

// etgdev.de：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

func EtgdevDeGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "etgdev-de",
		Email:   local + "@etgdev.de",
	}, nil
}

func EtgdevDeGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
