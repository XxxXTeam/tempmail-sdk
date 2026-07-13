package provider

// thisisnotmyrealemail.com：mailinator 官方姊妹域名，MX 指向 mail.mailinator.com。

// ThisisnotmyrealemailComGenerate 创建 thisisnotmyrealemail.com 临时邮箱
func ThisisnotmyrealemailComGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "thisisnotmyrealemail-com",
		Email:   local + "@thisisnotmyrealemail.com",
	}, nil
}

// ThisisnotmyrealemailComGetEmails 读取邮件（复用 mailinator public API）
func ThisisnotmyrealemailComGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
