package provider

func MingyuekejiOnlineGenerate() (*CreatedMailbox, error) {
	local := randomStr(10)
	email := local + "@mingyuekeji.online"
	return &CreatedMailbox{Channel: "mingyuekeji-online", Email: email, Token: email}, nil
}
func MingyuekejiOnlineGetEmails(email string) ([]NormEmail, error) { return MailmomyGetEmails(email) }
