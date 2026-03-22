package tempemail

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"time"

	http "github.com/bogdanfinn/fhttp"
)

const linshiEmailBaseURL = "https://www.linshi-email.com/api/v1"

type linshiEmailGenerateResponse struct {
	Data struct {
		Email   string `json:"email"`
		Expired int64  `json:"expired"`
	} `json:"data"`
	Status string `json:"status"`
}

type linshiEmailEmailsResponse struct {
	List   []json.RawMessage `json:"list"`
	Status string            `json:"status"`
}

func linshiEmailGenerate() (*EmailInfo, error) {
	apiKey := RandomSyntheticLinshiAPIPathKey()
	reqBody := []byte("{}")

	req, err := http.NewRequest("POST", linshiEmailBaseURL+"/email/"+apiKey, bytes.NewReader(reqBody))
	if err != nil {
		return nil, err
	}

	req.Header.Set("User-Agent", GetCurrentUA())
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Origin", "https://www.linshi-email.com")
	req.Header.Set("Referer", "https://www.linshi-email.com/")
	req.Header.Set("DNT", "1")

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if err := checkHTTPStatus(resp, "linshi-email generate"); err != nil {
		return nil, err
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var result linshiEmailGenerateResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}

	if result.Status != "ok" {
		return nil, fmt.Errorf("failed to generate email")
	}

	return &EmailInfo{
		Channel:   ChannelLinshiEmail,
		Email:     result.Data.Email,
		token:     apiKey,
		ExpiresAt: result.Data.Expired,
	}, nil
}

func linshiEmailGetEmails(apiPathKey string, email string) ([]Email, error) {
	if apiPathKey == "" {
		return nil, fmt.Errorf("internal error: api path key missing for linshi-email")
	}
	encodedEmail := url.QueryEscape(email)
	timestamp := time.Now().UnixMilli()

	reqURL := fmt.Sprintf("%s/refreshmessage/%s/%s?t=%d", linshiEmailBaseURL, apiPathKey, encodedEmail, timestamp)

	req, err := http.NewRequest("GET", reqURL, nil)
	if err != nil {
		return nil, err
	}

	req.Header.Set("User-Agent", GetCurrentUA())
	req.Header.Set("Referer", "https://www.linshi-email.com/")
	req.Header.Set("DNT", "1")

	client := HTTPClient()
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if err := checkHTTPStatus(resp, "linshi-email get emails"); err != nil {
		return nil, err
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var result linshiEmailEmailsResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}

	if result.Status != "ok" {
		return nil, fmt.Errorf("failed to get emails")
	}

	return normalizeRawEmails(result.List, email)
}
