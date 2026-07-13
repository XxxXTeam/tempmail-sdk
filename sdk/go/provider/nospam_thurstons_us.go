package provider

// nospam.thurstons.us：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

func NospamThurstonsUsGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "nospam-thurstons-us",
		Email:   local + "@nospam.thurstons.us",
	}, nil
}

func NospamThurstonsUsGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
