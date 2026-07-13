package provider

func M887AtGenerate() (*CreatedMailbox, error) {
	return &CreatedMailbox{Channel: "m-887-at", Email: randomStr(12) + "@m.887.at"}, nil
}
func M887AtGetEmails(email string) ([]NormEmail, error) { return MailinatorGetEmails(email) }
