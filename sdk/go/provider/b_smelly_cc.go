package provider

func BSmellyCcGenerate() (*CreatedMailbox, error) {
	return &CreatedMailbox{Channel: "b-smelly-cc", Email: randomStr(12) + "@b.smelly.cc"}, nil
}
func BSmellyCcGetEmails(email string) ([]NormEmail, error) { return MailinatorGetEmails(email) }
