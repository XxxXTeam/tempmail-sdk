package provider

// min.burningfish.net：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

func MinBurningfishNetGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "min-burningfish-net",
		Email:   local + "@min.burningfish.net",
	}, nil
}

func MinBurningfishNetGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
