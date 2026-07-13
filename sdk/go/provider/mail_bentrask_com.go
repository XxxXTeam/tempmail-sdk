package provider

func MailBentraskComGenerate() (*CreatedMailbox, error) {
	return &CreatedMailbox{Channel: "mail-bentrask-com", Email: randomStr(12) + "@mail.bentrask.com"}, nil
}
func MailBentraskComGetEmails(email string) ([]NormEmail, error) { return MailinatorGetEmails(email) }
