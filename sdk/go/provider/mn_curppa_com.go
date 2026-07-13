package provider

func MnCurppaComGenerate() (*CreatedMailbox, error) {
	return &CreatedMailbox{Channel: "mn-curppa-com", Email: randomStr(12) + "@mn.curppa.com"}, nil
}
func MnCurppaComGetEmails(email string) ([]NormEmail, error) { return MailinatorGetEmails(email) }
