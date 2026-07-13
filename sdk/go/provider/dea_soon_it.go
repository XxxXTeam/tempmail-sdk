package provider

func DeaSoonItGenerate() (*CreatedMailbox, error) {
	return &CreatedMailbox{Channel: "dea-soon-it", Email: randomStr(12) + "@dea.soon.it"}, nil
}
func DeaSoonItGetEmails(email string) ([]NormEmail, error) { return MailinatorGetEmails(email) }
