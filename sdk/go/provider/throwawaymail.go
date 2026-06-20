package provider

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const throwawaymailBase = "https://throwawaymail.app/api"

type throwawaymailMailboxResponse struct {
	MailboxID string `json:"mailbox_id"`
	Address   string `json:"address"`
	ExpiresAt string `json:"expires_at"`
	CreatedAt string `json:"created_at"`
}

type throwawaymailMessageSummary struct {
	MessageID   string  `json:"message_id"`
	FromAddress string  `json:"from_address"`
	FromName    *string `json:"from_name"`
	Subject     string  `json:"subject"`
	ReceivedAt  string  `json:"received_at"`
	Size        int     `json:"size"`
	Read        bool    `json:"read"`
}

type throwawaymailMessageDetail struct {
	throwawaymailMessageSummary
	ToAddresses  []string `json:"to_addresses"`
	CCAddresses  []string `json:"cc_addresses"`
	BCCAddresses []string `json:"bcc_addresses"`
	ReplyTo      *string  `json:"reply_to"`
	Text         *string  `json:"text"`
	HTML         *string  `json:"html"`
}

func throwawaymailJSON(method, u string, body []byte, out any) error {
	req, err := http.NewRequest(method, u, bytes.NewReader(body))
	if err != nil {
		return err
	}
	req.Header.Set("Accept", "application/json")
	if body != nil {
		req.Header.Set("Content-Type", "application/json")
	}
	resp, err := HTTPClient().Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return fmt.Errorf("throwawaymail: http %d", resp.StatusCode)
	}
	if out == nil {
		return nil
	}
	return json.Unmarshal(raw, out)
}

func throwawaymailRaw(summary throwawaymailMessageSummary, detail *throwawaymailMessageDetail, recipient string) map[string]any {
	raw := map[string]any{
		"id":           summary.MessageID,
		"messageId":    summary.MessageID,
		"from_address": summary.FromAddress,
		"subject":      summary.Subject,
		"received_at":  summary.ReceivedAt,
		"read":         summary.Read,
		"to":           recipient,
		"size":         summary.Size,
	}
	if summary.FromName != nil {
		raw["fromName"] = *summary.FromName
	}
	if detail != nil {
		if len(detail.ToAddresses) > 0 {
			raw["to"] = detail.ToAddresses[0]
		}
		if detail.Text != nil {
			raw["text"] = *detail.Text
		}
		if detail.HTML != nil {
			raw["html"] = *detail.HTML
		}
	}
	return raw
}

func ThrowawaymailGenerate() (*CreatedMailbox, error) {
	var data throwawaymailMailboxResponse
	if err := throwawaymailJSON(http.MethodPost, throwawaymailBase+"/mailboxes", nil, &data); err != nil {
		return nil, err
	}
	mailboxID := strings.TrimSpace(data.MailboxID)
	email := strings.TrimSpace(data.Address)
	if mailboxID == "" || email == "" || !strings.Contains(email, "@") {
		return nil, fmt.Errorf("throwawaymail: invalid mailbox response")
	}
	return &CreatedMailbox{
		Channel:   "throwawaymail",
		Email:     email,
		Token:     mailboxID,
		ExpiresAt: data.ExpiresAt,
		CreatedAt: data.CreatedAt,
	}, nil
}

func throwawaymailFetchDetail(mailboxID, messageID string) (*throwawaymailMessageDetail, error) {
	u := fmt.Sprintf(
		"%s/mailboxes/%s/messages/%s",
		throwawaymailBase,
		url.PathEscape(mailboxID),
		url.PathEscape(messageID),
	)
	var detail throwawaymailMessageDetail
	if err := throwawaymailJSON(http.MethodGet, u, nil, &detail); err != nil {
		return nil, err
	}
	return &detail, nil
}

func ThrowawaymailGetEmails(mailboxID, email string) ([]NormEmail, error) {
	mailboxID = strings.TrimSpace(mailboxID)
	email = strings.TrimSpace(email)
	if mailboxID == "" {
		return nil, fmt.Errorf("throwawaymail: empty mailbox id")
	}
	if email == "" {
		return nil, fmt.Errorf("throwawaymail: empty email")
	}

	u := fmt.Sprintf("%s/mailboxes/%s/messages", throwawaymailBase, url.PathEscape(mailboxID))
	var rows []throwawaymailMessageSummary
	if err := throwawaymailJSON(http.MethodGet, u, nil, &rows); err != nil {
		return nil, err
	}
	out := make([]NormEmail, 0, len(rows))
	for _, row := range rows {
		var detail *throwawaymailMessageDetail
		if strings.TrimSpace(row.MessageID) != "" {
			d, err := throwawaymailFetchDetail(mailboxID, row.MessageID)
			if err == nil {
				detail = d
			}
		}
		out = append(out, NormalizeMap(throwawaymailRaw(row, detail, email), email))
	}
	return out, nil
}
