package provider

func N16888888CyouGenerate() (*CreatedMailbox, error) {
	local := randomStr(10)
	email := local + "@16888888.cyou"
	return &CreatedMailbox{Channel: "16888888-cyou", Email: email, Token: email}, nil
}
func N16888888CyouGetEmails(email string) ([]NormEmail, error) { return MailmomyGetEmails(email) }
