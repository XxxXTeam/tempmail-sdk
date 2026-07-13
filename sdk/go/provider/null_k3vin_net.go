package provider

// null.k3vin.net：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

func NullK3vinNetGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "null-k3vin-net",
		Email:   local + "@null.k3vin.net",
	}, nil
}

func NullK3vinNetGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
