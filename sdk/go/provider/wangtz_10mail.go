package provider

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"strings"
	"time"

	http "github.com/bogdanfinn/fhttp"
)

const wangtz10Origin = "https://10mail.wangtz.cn"
const wangtz10MailDomain = "wangtz.cn"

func wangtz10JSONHeaders(req *http.Request) {
	req.Header.Set("User-Agent", GetCurrentUA())
	req.Header.Set("Accept", "*/*")
	req.Header.Set("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6")
	req.Header.Set("Content-Type", "application/json; charset=utf-8")
	req.Header.Set("Origin", wangtz10Origin)
	req.Header.Set("Referer", wangtz10Origin+"/")
	req.Header.Set("token", "null")
	req.Header.Set("Authorization", "null")
}

// wangtz10FormatAddrField 解析 emailList 中 mailparser 风格的地址对象：{ text, value:[{address,name}] }
func wangtz10FormatAddrField(v interface{}) string {
	m, ok := v.(map[string]interface{})
	if !ok {
		return ""
	}
	if t, ok := m["text"].(string); ok {
		if s := strings.TrimSpace(t); s != "" {
			return s
		}
	}
	arr, ok := m["value"].([]interface{})
	if !ok || len(arr) == 0 {
		return ""
	}
	vm, ok := arr[0].(map[string]interface{})
	if !ok {
		return ""
	}
	addr, _ := vm["address"].(string)
	addr = strings.TrimSpace(addr)
	if addr == "" {
		return ""
	}
	name, _ := vm["name"].(string)
	name = strings.TrimSpace(name)
	if name != "" && !strings.EqualFold(name, addr) {
		return fmt.Sprintf("%s <%s>", name, addr)
	}
	return addr
}

func wangtz10FlattenMessage(raw map[string]interface{}) map[string]interface{} {
	flat := make(map[string]interface{})
	for k, v := range raw {
		flat[k] = v
	}
	if mid, ok := raw["messageId"]; ok && mid != nil {
		flat["id"] = fmt.Sprintf("%v", mid)
	}
	if s := wangtz10FormatAddrField(raw["from"]); s != "" {
		flat["from"] = s
	}
	if s := wangtz10FormatAddrField(raw["to"]); s != "" {
		flat["to"] = s
	}
	return flat
}

// TenmailWangtzGenerate 调用 /api/tempMail；opts.Domain 非空时作为 emailName（本地部分），否则服务端随机。
func TenmailWangtzGenerate(domain *string) (*CreatedMailbox, error) {
	emailName := ""
	if domain != nil {
		s := strings.TrimSpace(*domain)
		if i := strings.Index(s, "@"); i >= 0 {
			s = strings.TrimSpace(s[:i])
		}
		emailName = s
	}
	payload := map[string]string{"emailName": emailName}
	raw, err := json.Marshal(payload)
	if err != nil {
		return nil, err
	}
	req, err := http.NewRequest("POST", wangtz10Origin+"/api/tempMail", bytes.NewReader(raw))
	if err != nil {
		return nil, err
	}
	wangtz10JSONHeaders(req)

	clientFn := HTTPClientTenmailWangtz
	if clientFn == nil {
		clientFn = HTTPClient
	}
	resp, err := clientFn().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "10mail-wangtz generate"); err != nil {
		return nil, err
	}
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	var wrap struct {
		Code int `json:"code"`
		Data *struct {
			MailName string  `json:"mailName"`
			EndTime  float64 `json:"endTime"`
		} `json:"data"`
		Msg string `json:"msg"`
	}
	if err := json.Unmarshal(body, &wrap); err != nil {
		return nil, err
	}
	if wrap.Code != 0 || wrap.Data == nil {
		return nil, fmt.Errorf("10mail-wangtz: %s", strings.TrimSpace(wrap.Msg))
	}
	local := strings.TrimSpace(wrap.Data.MailName)
	if local == "" {
		return nil, fmt.Errorf("10mail-wangtz: empty mailName")
	}
	addr := local + "@" + wangtz10MailDomain
	var exp any
	if wrap.Data.EndTime > 0 {
		exp = time.UnixMilli(int64(wrap.Data.EndTime)).UTC().Format(time.RFC3339)
	}
	return &CreatedMailbox{
		Channel:   "10mail-wangtz",
		Email:     addr,
		Token:     "",
		ExpiresAt: exp,
	}, nil
}

// TenmailWangtzGetEmails 调用 /api/emailList；无需 token。
func TenmailWangtzGetEmails(email, _ string) ([]NormEmail, error) {
	payload := map[string]string{"emailName": strings.TrimSpace(email)}
	raw, err := json.Marshal(payload)
	if err != nil {
		return nil, err
	}
	req, err := http.NewRequest("POST", wangtz10Origin+"/api/emailList", bytes.NewReader(raw))
	if err != nil {
		return nil, err
	}
	wangtz10JSONHeaders(req)

	clientFn := HTTPClientTenmailWangtz
	if clientFn == nil {
		clientFn = HTTPClient
	}
	resp, err := clientFn().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "10mail-wangtz inbox"); err != nil {
		return nil, err
	}
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	var list []map[string]interface{}
	if err := json.Unmarshal(body, &list); err != nil {
		return nil, fmt.Errorf("10mail-wangtz: inbox json: %w", err)
	}
	out := make([]NormEmail, 0, len(list))
	for _, item := range list {
		if item == nil {
			continue
		}
		out = append(out, NormalizeMap(wangtz10FlattenMessage(item), email))
	}
	return out, nil
}
