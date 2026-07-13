package provider

func Nuxh62SpaceGenerate() (*CreatedMailbox, error) {
	local := randomStr(10)
	email := local + "@nuxh62.space"
	return &CreatedMailbox{Channel: "nuxh62-space", Email: email, Token: email}, nil
}
func Nuxh62SpaceGetEmails(email string) ([]NormEmail, error) { return MailmomyGetEmails(email) }
