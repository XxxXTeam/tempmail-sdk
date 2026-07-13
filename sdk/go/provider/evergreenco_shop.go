package provider

func EvergreencoShopGenerate() (*CreatedMailbox, error) {
	local := randomStr(10)
	email := local + "@evergreenco.shop"
	return &CreatedMailbox{Channel: "evergreenco-shop", Email: email, Token: email}, nil
}
func EvergreencoShopGetEmails(email string) ([]NormEmail, error) { return MailmomyGetEmails(email) }
