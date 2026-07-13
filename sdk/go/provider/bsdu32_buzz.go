package provider

func Bsdu32BuzzGenerate() (*CreatedMailbox, error) {
	local := randomStr(10)
	email := local + "@bsdu32.buzz"
	return &CreatedMailbox{Channel: "bsdu32-buzz", Email: email, Token: email}, nil
}
func Bsdu32BuzzGetEmails(email string) ([]NormEmail, error) { return MailmomyGetEmails(email) }
