package provider

// sink.fblay.com：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

func SinkFblayComGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "sink-fblay-com",
		Email:   local + "@sink.fblay.com",
	}, nil
}

func SinkFblayComGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
