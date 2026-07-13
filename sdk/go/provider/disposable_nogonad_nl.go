package provider

func DisposableNogonadNlGenerate() (*CreatedMailbox, error) {
	return &CreatedMailbox{Channel: "disposable-nogonad-nl", Email: randomStr(12) + "@disposable.nogonad.nl"}, nil
}
func DisposableNogonadNlGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
