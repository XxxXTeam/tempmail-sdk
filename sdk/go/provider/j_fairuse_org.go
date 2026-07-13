package provider

func JFairuseOrgGenerate() (*CreatedMailbox, error) {
	return &CreatedMailbox{Channel: "j-fairuse-org", Email: randomStr(12) + "@j.fairuse.org"}, nil
}
func JFairuseOrgGetEmails(email string) ([]NormEmail, error) { return MailinatorGetEmails(email) }
