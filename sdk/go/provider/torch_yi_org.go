package provider

// torch.yi.org：mailinator 官方姊妹子域名，MX 指向 mail.mailinator.com。

func TorchYiOrgGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "torch-yi-org",
		Email:   local + "@torch.yi.org",
	}, nil
}

func TorchYiOrgGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
