package provider

func Doxu243BuzzGenerate() (*CreatedMailbox, error) {
	local := randomStr(10)
	email := local + "@doxu243.buzz"
	return &CreatedMailbox{Channel: "doxu243-buzz", Email: email, Token: email}, nil
}
func Doxu243BuzzGetEmails(email string) ([]NormEmail, error) { return MailmomyGetEmails(email) }
