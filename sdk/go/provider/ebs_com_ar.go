package provider

// ebs.com.ar：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

func EbsComArGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "ebs-com-ar",
		Email:   local + "@ebs.com.ar",
	}, nil
}

func EbsComArGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
