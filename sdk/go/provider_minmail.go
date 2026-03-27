package tempemail

import (
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"time"

	http "github.com/bogdanfinn/fhttp"
)

const minmailAPIBase = "https://minmail.app/api"

type minmailAddressResp struct {
	Address string `json:"address"`
	Expire  int    `json:"expire"`
}

type minmailListResp struct {
	Message []minmailMessage `json:"message"`
}

type minmailMessage struct {
	ID      string `json:"id"`
	From    string `json:"from"`
	To      string `json:"to"`
	Subject string `json:"subject"`
	Preview string `json:"preview"`
	Content string `json:"content"`
	Date    string `json:"date"`
	IsRead  bool   `json:"isRead"`
}

func minmailRandomSeg(n int) string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, n)
	for i := range b {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}

func minmailVisitorID() string {
	return fmt.Sprintf("%s-%s-%s-%s-%s",
		minmailRandomSeg(8), minmailRandomSeg(4), minmailRandomSeg(4), minmailRandomSeg(4), minmailRandomSeg(12))
}

func minmailCookieHeader() string {
	now := time.Now().UnixMilli()
	rnd := rand.Intn(1000000)
	gaID := fmt.Sprintf("GA1.1.%d.%d", now, rnd)
	/* 与 TypeScript SDK 保持一致 */
	return fmt.Sprintf("_ga=GA1.1.%s; _ga_DFGB8WF1WG=GS2.1.s%d$o1$g0$t%d$j60$l0$h0", gaID, now, now)
}

func minmailSetHeaders(req *http.Request, visitorID string) {
	req.Header.Set("Accept", "*/*")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6,zh-TW;q=0.5")
	req.Header.Set("Origin", "https://minmail.app")
	req.Header.Set("Referer", "https://minmail.app/cn")
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0")
	req.Header.Set("sec-ch-ua", `"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"`)
	req.Header.Set("sec-ch-ua-mobile", "?0")
	req.Header.Set("sec-ch-ua-platform", `"Windows"`)
	req.Header.Set("sec-fetch-dest", "empty")
	req.Header.Set("sec-fetch-mode", "cors")
	req.Header.Set("sec-fetch-site", "same-origin")
	req.Header.Set("visitor-id", visitorID)
	req.Header.Set("Cookie", minmailCookieHeader())
}

func minmailGenerate() (*EmailInfo, error) {
	vid := minmailVisitorID()
	req, err := http.NewRequest("GET", minmailAPIBase+"/mail/address?refresh=true&expire=1440&part=main", nil)
	if err != nil {
		return nil, err
	}
	minmailSetHeaders(req, vid)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("minmail address: %d", resp.StatusCode)
	}

	var data minmailAddressResp
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, err
	}
	if data.Address == "" {
		return nil, fmt.Errorf("minmail: empty address")
	}

	expiresAt := time.Now().Add(time.Duration(data.Expire) * time.Minute).UnixMilli()
	return &EmailInfo{
		Channel:   ChannelMinmail,
		Email:     data.Address,
		token:     vid,
		ExpiresAt: expiresAt,
	}, nil
}

func minmailGetEmails(email, visitorID string) ([]Email, error) {
	if visitorID == "" {
		visitorID = minmailVisitorID()
	}
	req, err := http.NewRequest("GET", minmailAPIBase+"/mail/list?part=main", nil)
	if err != nil {
		return nil, err
	}
	minmailSetHeaders(req, visitorID)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("minmail list: %d", resp.StatusCode)
	}

	var data minmailListResp
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, err
	}

	var emails []Email
	for _, raw := range data.Message {
		if raw.To != "" && raw.To != email {
			continue
		}
		flat := map[string]interface{}{
			"id":      raw.ID,
			"from":    raw.From,
			"to":      raw.To,
			"subject": raw.Subject,
			"text":    raw.Preview,
			"html":    raw.Content,
			"date":    raw.Date,
			"isRead":  raw.IsRead,
		}
		emails = append(emails, normalizeRawEmail(flat, email))
	}
	return emails, nil
}
