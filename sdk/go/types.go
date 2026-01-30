package tempemail

type Channel string

const (
	ChannelTempmail     Channel = "tempmail"
	ChannelLinshiEmail  Channel = "linshi-email"
	ChannelTempmailLol  Channel = "tempmail-lol"
	ChannelChatgptOrgUk Channel = "chatgpt-org-uk"
)

type EmailInfo struct {
	Channel   Channel `json:"channel"`
	Email     string  `json:"email"`
	Token     string  `json:"token,omitempty"`
	ExpiresAt any     `json:"expiresAt,omitempty"`
	CreatedAt string  `json:"createdAt,omitempty"`
}

type Email struct {
	ID           any    `json:"id,omitempty"`
	Eid          string `json:"eid,omitempty"`
	MongoID      string `json:"_id,omitempty"`
	From         string `json:"from,omitempty"`
	FromAddress  string `json:"from_address,omitempty"`
	FromName     string `json:"from_name,omitempty"`
	AddressFrom  string `json:"address_from,omitempty"`
	NameFrom     string `json:"name_from,omitempty"`
	To           string `json:"to,omitempty"`
	NameTo       string `json:"name_to,omitempty"`
	EmailAddress string `json:"email_address,omitempty"`
	Subject      string `json:"subject,omitempty"`
	ESubject     string `json:"e_subject,omitempty"`
	Body         string `json:"body,omitempty"`
	Text         string `json:"text,omitempty"`
	Content      string `json:"content,omitempty"`
	HTML         string `json:"html,omitempty"`
	HTMLContent  string `json:"html_content,omitempty"`
	Date         any    `json:"date,omitempty"`
	EDate        int64  `json:"e_date,omitempty"`
	Timestamp    int64  `json:"timestamp,omitempty"`
	ReceivedAt   string `json:"received_at,omitempty"`
	CreatedAt    string `json:"created_at,omitempty"`
	CreatedAtAlt string `json:"createdAt,omitempty"`
	IsRead       int    `json:"is_read,omitempty"`
	HasHTML      bool   `json:"has_html,omitempty"`
}

type GetEmailsResult struct {
	Channel Channel `json:"channel"`
	Email   string  `json:"email"`
	Emails  []Email `json:"emails"`
	Success bool    `json:"success"`
}

type GenerateEmailOptions struct {
	Channel  Channel
	Duration int
	Domain   *string
}

type GetEmailsOptions struct {
	Channel Channel
	Email   string
	Token   string
}
