package provider

func MingyukejiLolGenerate() (*CreatedMailbox, error) {
	local := randomStr(10)
	email := local + "@mingyukeji.lol"
	return &CreatedMailbox{Channel: "mingyukeji-lol", Email: email, Token: email}, nil
}
func MingyukejiLolGetEmails(email string) ([]NormEmail, error) { return MailmomyGetEmails(email) }
