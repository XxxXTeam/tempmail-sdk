package provider

// blackhole.djurby.se：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

func BlackholeDjurbySeGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "blackhole-djurby-se",
		Email:   local + "@blackhole.djurby.se",
	}, nil
}

func BlackholeDjurbySeGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
