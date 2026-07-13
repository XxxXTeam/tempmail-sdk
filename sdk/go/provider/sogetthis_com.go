package provider

// sogetthis.com：mailinator 官方姊妹域名，MX 指向 mail.mailinator.com。
// 读信复用 mailinator 的 domain=public API（自动聚合所有姊妹域名收件）。

// SogetthisComGenerate 创建 sogetthis.com 临时邮箱
func SogetthisComGenerate() (*CreatedMailbox, error) {
	local := randomStr(12)
	return &CreatedMailbox{
		Channel: "sogetthis-com",
		Email:   local + "@sogetthis.com",
	}, nil
}

// SogetthisComGetEmails 读取 sogetthis.com 邮件（复用 mailinator public API）
func SogetthisComGetEmails(email string) ([]NormEmail, error) {
	return MailinatorGetEmails(email)
}
