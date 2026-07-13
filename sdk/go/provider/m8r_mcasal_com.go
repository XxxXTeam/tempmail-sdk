package provider

func M8rMcasalComGenerate() (*CreatedMailbox, error) {
	return &CreatedMailbox{Channel: "m8r-mcasal-com", Email: randomStr(12) + "@m8r.mcasal.com"}, nil
}
func M8rMcasalComGetEmails(email string) ([]NormEmail, error) { return MailinatorGetEmails(email) }
