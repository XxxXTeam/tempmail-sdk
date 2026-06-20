package provider

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"regexp"
	"sync"

	http "github.com/bogdanfinn/fhttp"
)

const tempMailIoBaseURL = "https://api.internal.temp-mail.io/api/v3"
const tempMailIoPageURL = "https://temp-mail.io/en"

var (
	tempMailIoHeaderMu     sync.Mutex
	tempMailIoCachedHeader string
)

type tempMailIoGenerateResponse struct {
	Email string `json:"email"`
	Token string `json:"token"`
}

func tempMailIoFetchCorsHeader() string {
	tempMailIoHeaderMu.Lock()
	defer tempMailIoHeaderMu.Unlock()

	if tempMailIoCachedHeader != "" {
		return tempMailIoCachedHeader
	}

	req, err := http.NewRequest("GET", tempMailIoPageURL, nil)
	if err == nil {
		req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36")
		resp, reqErr := HTTPClient().Do(req)
		if reqErr == nil && resp != nil {
			body, readErr := io.ReadAll(resp.Body)
			resp.Body.Close()
			if readErr == nil {
				re := regexp.MustCompile(`mobileTestingHeader\s*:\s*"([^"]+)"`)
				if match := re.FindSubmatch(body); len(match) == 2 {
					tempMailIoCachedHeader = string(match[1])
					return tempMailIoCachedHeader
				}
			}
		} else if resp != nil {
			resp.Body.Close()
		}
	}

	tempMailIoCachedHeader = "1"
	return tempMailIoCachedHeader
}

func tempMailIoApplyHeaders(req *http.Request) {
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Application-Name", "web")
	req.Header.Set("Application-Version", "4.0.0")
	req.Header.Set("X-CORS-Header", tempMailIoFetchCorsHeader())
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0")
	req.Header.Set("Origin", "https://temp-mail.io")
	req.Header.Set("Referer", "https://temp-mail.io/")
}

func TempMailIoGenerate() (*CreatedMailbox, error) {
	reqBody, _ := json.Marshal(map[string]int{
		"min_name_length": 10,
		"max_name_length": 10,
	})

	req, err := http.NewRequest("POST", tempMailIoBaseURL+"/email/new", bytes.NewReader(reqBody))
	if err != nil {
		return nil, err
	}
	tempMailIoApplyHeaders(req)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("temp-mail-io generate: %d", resp.StatusCode)
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var data tempMailIoGenerateResponse
	if err := json.Unmarshal(body, &data); err != nil {
		return nil, err
	}
	if data.Email == "" || data.Token == "" {
		return nil, fmt.Errorf("temp-mail-io: invalid generate response")
	}

	return &CreatedMailbox{Channel: "temp-mail-io", Email: data.Email, Token: data.Token}, nil
}

func TempMailIoGetEmails(email string) ([]NormEmail, error) {
	req, err := http.NewRequest("GET", tempMailIoBaseURL+"/email/"+email+"/messages", nil)
	if err != nil {
		return nil, err
	}
	tempMailIoApplyHeaders(req)

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("temp-mail-io messages: %d", resp.StatusCode)
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var rows []map[string]interface{}
	if err := json.Unmarshal(body, &rows); err != nil {
		return nil, err
	}

	emails := make([]NormEmail, 0, len(rows))
	for _, raw := range rows {
		emails = append(emails, NormalizeMap(raw, email))
	}
	return emails, nil
}
