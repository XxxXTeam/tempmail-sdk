package provider

func Fwd2mEszettEsGenerate() (*CreatedMailbox, error) {
	return &CreatedMailbox{Channel: "fwd2m-eszett-es", Email: randomStr(12) + "@fwd2m.eszett.es"}, nil
}
func Fwd2mEszettEsGetEmails(email string) ([]NormEmail, error) { return MailinatorGetEmails(email) }
