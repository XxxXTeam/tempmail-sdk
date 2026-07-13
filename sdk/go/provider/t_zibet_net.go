package provider

func TZibetNetGenerate() (*CreatedMailbox, error) {
	return &CreatedMailbox{Channel: "t-zibet-net", Email: randomStr(12) + "@t.zibet.net"}, nil
}
func TZibetNetGetEmails(email string) ([]NormEmail, error) { return MailinatorGetEmails(email) }
