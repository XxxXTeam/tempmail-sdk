package provider

func ProidCloudIpCcGenerate() (*CreatedMailbox, error) {
	local := randomStr(10)
	email := local + "@proid.cloud-ip.cc"
	return &CreatedMailbox{Channel: "proid-cloud-ip-cc", Email: email, Token: email}, nil
}
func ProidCloudIpCcGetEmails(email string) ([]NormEmail, error) { return MailmomyGetEmails(email) }
