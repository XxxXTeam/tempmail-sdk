package provider

// block.bdea.cc：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

func BlockBdeaCcGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "block-bdea-cc",
		Email:   local + "@block.bdea.cc",
	}, nil
}

func BlockBdeaCcGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
