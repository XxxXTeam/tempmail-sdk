package provider

func EasymeProGenerate() (*CreatedMailbox, error) {
	local := randomStr(10)
	email := local + "@easyme.pro"
	return &CreatedMailbox{Channel: "easyme-pro", Email: email, Token: email}, nil
}
func EasymeProGetEmails(email string) ([]NormEmail, error) { return MailmomyGetEmails(email) }
