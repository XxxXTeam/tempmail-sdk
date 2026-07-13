package provider

// spam.hortuk.ovh：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

func SpamHortukOvhGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "spam-hortuk-ovh",
		Email:   local + "@spam.hortuk.ovh",
	}, nil
}

func SpamHortukOvhGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
