package provider

func MailinatorzzmoooComGenerate() (*CreatedMailbox, error) {
	return &CreatedMailbox{Channel: "mailinatorzz-mooo-com", Email: randomStr(12) + "@mailinatorzz.mooo.com"}, nil
}
func MailinatorzzmoooComGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
