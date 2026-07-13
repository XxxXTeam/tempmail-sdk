package provider

// mi.meon.be：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

func MiMeonBeGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "mi-meon-be",
		Email:   local + "@mi.meon.be",
	}, nil
}

func MiMeonBeGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
