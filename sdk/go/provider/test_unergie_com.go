package provider

// test.unergie.com：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

func TestUnergieComGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "test-unergie-com",
		Email:   local + "@test.unergie.com",
	}, nil
}

func TestUnergieComGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
