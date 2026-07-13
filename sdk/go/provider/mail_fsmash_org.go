package provider

// mail.fsmash.org：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

func MailFsmashOrgGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "mail-fsmash-org",
		Email:   local + "@mail.fsmash.org",
	}, nil
}

func MailFsmashOrgGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
