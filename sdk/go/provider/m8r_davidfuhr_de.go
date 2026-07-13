package provider

// m8r.davidfuhr.de：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

func M8rDavidfuhrDeGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "m8r-davidfuhr-de",
		Email:   local + "@m8r.davidfuhr.de",
	}, nil
}

func M8rDavidfuhrDeGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
