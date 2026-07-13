package provider

func RamjaneMoooComGenerate() (*CreatedMailbox, error) {
	return &CreatedMailbox{Channel: "ramjane-mooo-com", Email: randomStr(12) + "@ramjane.mooo.com"}, nil
}
func RamjaneMoooComGetEmails(email string) ([]NormEmail, error) { return MailinatorGetEmails(email) }
