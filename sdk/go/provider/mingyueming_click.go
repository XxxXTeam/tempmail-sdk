package provider

func MingyuemingClickGenerate() (*CreatedMailbox, error) {
	local := randomStr(10)
	email := local + "@mingyueming.click"
	return &CreatedMailbox{Channel: "mingyueming-click", Email: email, Token: email}, nil
}
func MingyuemingClickGetEmails(email string) ([]NormEmail, error) { return MailmomyGetEmails(email) }
