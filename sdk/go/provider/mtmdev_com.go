package provider

// mtmdev.com：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

func MtmdevComGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "mtmdev-com",
		Email:   local + "@mtmdev.com",
	}, nil
}

func MtmdevComGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
