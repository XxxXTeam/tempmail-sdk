package provider

func SpWootAtGenerate() (*CreatedMailbox, error) {
	return &CreatedMailbox{Channel: "sp-woot-at", Email: randomStr(12) + "@sp.woot.at"}, nil
}
func SpWootAtGetEmails(email string) ([]NormEmail, error) { return MailinatorGetEmails(email) }
