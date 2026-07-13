package provider

func DisposableAlSudaniComGenerate() (*CreatedMailbox, error) {
	return &CreatedMailbox{Channel: "disposable-al-sudani-com", Email: randomStr(12) + "@disposable.al-sudani.com"}, nil
}
func DisposableAlSudaniComGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
