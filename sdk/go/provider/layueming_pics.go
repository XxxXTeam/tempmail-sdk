package provider

func LayuemingPicsGenerate() (*CreatedMailbox, error) {
	local := randomStr(10)
	email := local + "@layueming.pics"
	return &CreatedMailbox{Channel: "layueming-pics", Email: email, Token: email}, nil
}
func LayuemingPicsGetEmails(email string) ([]NormEmail, error) { return MailmomyGetEmails(email) }
