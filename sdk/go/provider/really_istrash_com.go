package provider

// really.istrash.com：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

func ReallyIstrashComGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "really-istrash-com",
		Email:   local + "@really.istrash.com",
	}, nil
}

func ReallyIstrashComGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
