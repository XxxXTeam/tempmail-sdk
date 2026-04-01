package tempemail

import (
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

const fakeLegalBase = "https://fake.legal"

func fakeLegalDefaultHeaders(req *http.Request) {
	req.Header.Set("Accept", "application/json, text/plain, */*")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
	req.Header.Set("Cache-Control", "no-cache")
	req.Header.Set("DNT", "1")
	req.Header.Set("Pragma", "no-cache")
	req.Header.Set("Referer", "https://fake.legal/")
	req.Header.Set("sec-ch-ua", `"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"`)
	req.Header.Set("sec-ch-ua-mobile", "?0")
	req.Header.Set("sec-ch-ua-platform", `"Windows"`)
	req.Header.Set("sec-fetch-dest", "empty")
	req.Header.Set("sec-fetch-mode", "cors")
	req.Header.Set("sec-fetch-site", "same-origin")
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0")
}

type fakeLegalDomainsResp struct {
	Domains []string `json:"domains"`
}

type fakeLegalNewResp struct {
	Success bool   `json:"success"`
	Address string `json:"address"`
}

type fakeLegalInboxResp struct {
	Success bool                     `json:"success"`
	Emails  []map[string]interface{} `json:"emails"`
}

func fakeLegalFetchDomains() ([]string, error) {
	req, err := http.NewRequest("GET", fakeLegalBase+"/api/domains", nil)
	if err != nil {
		return nil, err
	}
	fakeLegalDefaultHeaders(req)
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
		return nil, fmt.Errorf("fake-legal domains: %d", resp.StatusCode)
	}
	var dr fakeLegalDomainsResp
	if err := json.Unmarshal(body, &dr); err != nil {
		return nil, err
	}
	var out []string
	for _, d := range dr.Domains {
		s := strings.TrimSpace(d)
		if s != "" {
			out = append(out, s)
		}
	}
	if len(out) == 0 {
		return nil, fmt.Errorf("fake-legal: no domains")
	}
	return out, nil
}

func fakeLegalPickDomain(domains []string, preferred *string) string {
	if preferred != nil {
		p := strings.TrimSpace(*preferred)
		if p != "" {
			pl := strings.ToLower(p)
			for _, d := range domains {
				if strings.ToLower(d) == pl {
					return d
				}
			}
		}
	}
	return domains[rand.Intn(len(domains))]
}

func fakeLegalGenerate(domain *string) (*EmailInfo, error) {
	domains, err := fakeLegalFetchDomains()
	if err != nil {
		return nil, err
	}
	d := fakeLegalPickDomain(domains, domain)
	u := fmt.Sprintf("%s/api/inbox/new?domain=%s", fakeLegalBase, url.QueryEscape(d))
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, err
	}
	fakeLegalDefaultHeaders(req)
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
		return nil, fmt.Errorf("fake-legal new inbox: %d", resp.StatusCode)
	}
	var nr fakeLegalNewResp
	if err := json.Unmarshal(body, &nr); err != nil {
		return nil, err
	}
	if !nr.Success || strings.TrimSpace(nr.Address) == "" {
		return nil, fmt.Errorf("fake-legal: invalid new inbox response")
	}
	return &EmailInfo{
		Channel: ChannelFakeLegal,
		Email:   strings.TrimSpace(nr.Address),
	}, nil
}

func fakeLegalGetEmails(email string) ([]Email, error) {
	email = strings.TrimSpace(email)
	if email == "" {
		return nil, fmt.Errorf("fake-legal: empty email")
	}
	seg := url.PathEscape(email)
	u := fmt.Sprintf("%s/api/inbox/%s", fakeLegalBase, seg)
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, err
	}
	fakeLegalDefaultHeaders(req)
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
		return nil, fmt.Errorf("fake-legal inbox: %d", resp.StatusCode)
	}
	var ir fakeLegalInboxResp
	if err := json.Unmarshal(body, &ir); err != nil {
		return nil, err
	}
	if !ir.Success || ir.Emails == nil {
		return []Email{}, nil
	}
	emails := make([]Email, 0, len(ir.Emails))
	for _, raw := range ir.Emails {
		emails = append(emails, normalizeRawEmail(raw, email))
	}
	return emails, nil
}
