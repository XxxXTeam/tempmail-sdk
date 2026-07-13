package provider

func Notfond404MnGenerate() (*CreatedMailbox, error) {
	return &CreatedMailbox{Channel: "notfond-404-mn", Email: randomStr(12) + "@notfond.404.mn"}, nil
}
func Notfond404MnGetEmails(email string) ([]NormEmail, error) { return MailinatorGetEmails(email) }
