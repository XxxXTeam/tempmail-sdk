package provider

func N17666688ShopGenerate() (*CreatedMailbox, error) {
	local := randomStr(10)
	email := local + "@17666688.shop"
	return &CreatedMailbox{Channel: "17666688-shop", Email: email, Token: email}, nil
}
func N17666688ShopGetEmails(email string) ([]NormEmail, error) { return MailmomyGetEmails(email) }
