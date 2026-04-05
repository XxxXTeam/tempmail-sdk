package tempemail

import (
	"encoding/json"

	"github.com/XxxXTeam/tempmail-sdk/sdk/go/provider"
	tls_client "github.com/bogdanfinn/tls-client"
)

func init() {
	provider.HTTPClient = func() tls_client.HttpClient {
		return HTTPClient()
	}
	provider.HTTPClientTenmailWangtz = HTTPClientTenmailWangtz
	provider.HTTPClientNoRedirect = HTTPClientNoRedirect
	provider.HTTPClientNoCookieJar = HTTPClientNoCookieJar
	provider.CheckHTTPStatus = checkHTTPStatus
	provider.GetCurrentUA = GetCurrentUA
	provider.GetConfigSnapshot = func() provider.ConfigSnapshot {
		c := GetConfig()
		return provider.ConfigSnapshot{
			Proxy:                    c.Proxy,
			Timeout:                  int64(c.Timeout),
			Insecure:                 c.Insecure,
			DropmailAuthToken:        c.DropmailAuthToken,
			DropmailDisableAutoToken: c.DropmailDisableAutoToken,
			DropmailRenewLifetime:    c.DropmailRenewLifetime,
		}
	}
	provider.NormalizeMap = func(raw map[string]interface{}, recipientEmail string) provider.NormEmail {
		return emailToNorm(normalizeRawEmail(raw, recipientEmail))
	}
	provider.NormalizeRawMessages = func(rawMessages []json.RawMessage, recipientEmail string) ([]provider.NormEmail, error) {
		emails, err := normalizeRawEmails(rawMessages, recipientEmail)
		if err != nil {
			return nil, err
		}
		out := make([]provider.NormEmail, len(emails))
		for i := range emails {
			out[i] = emailToNorm(emails[i])
		}
		return out, nil
	}
}

func emailToNorm(e Email) provider.NormEmail {
	atts := make([]provider.NormAttachment, len(e.Attachments))
	for i, a := range e.Attachments {
		atts[i] = provider.NormAttachment{
			Filename:    a.Filename,
			Size:        a.Size,
			ContentType: a.ContentType,
			URL:         a.URL,
		}
	}
	return provider.NormEmail{
		ID:          e.ID,
		From:        e.From,
		To:          e.To,
		Subject:     e.Subject,
		Text:        e.Text,
		HTML:        e.HTML,
		Date:        e.Date,
		IsRead:      e.IsRead,
		Attachments: atts,
	}
}

func normToEmail(n provider.NormEmail) Email {
	atts := make([]EmailAttachment, len(n.Attachments))
	for i, a := range n.Attachments {
		atts[i] = EmailAttachment{
			Filename:    a.Filename,
			Size:        a.Size,
			ContentType: a.ContentType,
			URL:         a.URL,
		}
	}
	return Email{
		ID:          n.ID,
		From:        n.From,
		To:          n.To,
		Subject:     n.Subject,
		Text:        n.Text,
		HTML:        n.HTML,
		Date:        n.Date,
		IsRead:      n.IsRead,
		Attachments: atts,
	}
}

func normSliceToEmails(ns []provider.NormEmail) []Email {
	out := make([]Email, len(ns))
	for i := range ns {
		out[i] = normToEmail(ns[i])
	}
	return out
}

func mailboxToEmailInfo(m *provider.CreatedMailbox) *EmailInfo {
	if m == nil {
		return nil
	}
	return &EmailInfo{
		Channel:   Channel(m.Channel),
		Email:     m.Email,
		token:     m.Token,
		ExpiresAt: m.ExpiresAt,
		CreatedAt: m.CreatedAt,
	}
}

func fromMailbox(m *provider.CreatedMailbox, err error) (*EmailInfo, error) {
	if err != nil {
		return nil, err
	}
	return mailboxToEmailInfo(m), nil
}

func normEmailsResult(ns []provider.NormEmail, err error) ([]Email, error) {
	if err != nil {
		return nil, err
	}
	return normSliceToEmails(ns), nil
}
