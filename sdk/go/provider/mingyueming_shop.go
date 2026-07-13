package provider

func MingyuemingShopGenerate() (*CreatedMailbox, error) {
	local := randomStr(10)
	email := local + "@mingyueming.shop"
	return &CreatedMailbox{Channel: "mingyueming-shop", Email: email, Token: email}, nil
}
func MingyuemingShopGetEmails(email string) ([]NormEmail, error) { return MailmomyGetEmails(email) }
