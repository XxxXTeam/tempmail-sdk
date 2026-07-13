package provider

func RauxaSenyCatGenerate() (*CreatedMailbox, error) {
	return &CreatedMailbox{Channel: "rauxa-seny-cat", Email: randomStr(12) + "@rauxa.seny.cat"}, nil
}
func RauxaSenyCatGetEmails(email string) ([]NormEmail, error) { return MailinatorGetEmails(email) }
