package provider

// jama.trenet.eu：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

func JamaTrenetEuGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "jama-trenet-eu",
		Email:   local + "@jama.trenet.eu",
	}, nil
}

func JamaTrenetEuGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
