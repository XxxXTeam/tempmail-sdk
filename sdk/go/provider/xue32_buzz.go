package provider

func Xue32BuzzGenerate() (*CreatedMailbox, error) {
	local := randomStr(10)
	email := local + "@xue32.buzz"
	return &CreatedMailbox{Channel: "xue32-buzz", Email: email, Token: email}, nil
}
func Xue32BuzzGetEmails(email string) ([]NormEmail, error) { return MailmomyGetEmails(email) }
