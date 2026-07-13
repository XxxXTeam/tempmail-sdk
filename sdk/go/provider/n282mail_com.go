package provider

func N282mailComGenerate() (*CreatedMailbox, error) {
	local := randomStr(10)
	email := local + "@282mail.com"
	return &CreatedMailbox{Channel: "282mail-com", Email: email, Token: email}, nil
}
func N282mailComGetEmails(email string) ([]NormEmail, error) { return MailmomyGetEmails(email) }
