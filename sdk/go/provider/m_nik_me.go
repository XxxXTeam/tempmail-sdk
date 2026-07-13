package provider

// m.nik.me：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

func MNikMeGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "m-nik-me",
		Email:   local + "@m.nik.me",
	}, nil
}

func MNikMeGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
