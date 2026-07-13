package provider

func SbookPicsGenerate() (*CreatedMailbox, error) {
	local := randomStr(10)
	email := local + "@sbook.pics"
	return &CreatedMailbox{Channel: "sbook-pics", Email: email, Token: email}, nil
}
func SbookPicsGetEmails(email string) ([]NormEmail, error) { return MailmomyGetEmails(email) }
