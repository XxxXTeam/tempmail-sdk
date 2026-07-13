package provider

import (
	"encoding/base64"
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"net/url"
	"strings"

	http "github.com/bogdanfinn/fhttp"
)

// apihz（接口盒子）：需 id/key 凭据的临时邮箱，邮箱有效期 10 分钟。
// 读信须携带创建时的 pwd，故 token 同时保存 mail + pwd。

const (
	apihzBaseURL   = "https://cn.apihz.cn"
	apihzTokPrefix = "apihz1:"
	// apihz 官方公共账号（共享频次），未配置自有 id/key 时使用
	apihzPublicID  = "88888888"
	apihzPublicKey = "88888888"
	apihzUA        = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36"
)

// apihzDomains 可用收信域名（apihz 自研，MX 指向 mail.apimail.* 自建服务器）
var apihzDomains = []string{"apimail.email", "apimail.vip"}

// apihzCredentials 读取 apihz 调用凭据：优先配置/环境变量，回退官方公共账号
func apihzCredentials() (id, key string) {
	id, key = apihzPublicID, apihzPublicKey
	if GetConfigSnapshot != nil {
		cfg := GetConfigSnapshot()
		if s := strings.TrimSpace(cfg.ApihzID); s != "" {
			id = s
		}
		if s := strings.TrimSpace(cfg.ApihzKey); s != "" {
			key = s
		}
	}
	return id, key
}

func apihzRandomLocal(length int) string {
	const chars = "abcdefghijklmnopqrstuvwxyz0123456789"
	buf := make([]byte, length)
	for i := range buf {
		buf[i] = chars[rand.Intn(len(chars))]
	}
	return string(buf)
}

// apihzRandomPassword 生成 12 位随机密码（读信必须携带创建时的密码）
func apihzRandomPassword() string {
	const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
	buf := make([]byte, 12)
	for i := range buf {
		buf[i] = chars[rand.Intn(len(chars))]
	}
	return string(buf)
}

// apihzEncodeToken token 编码：前缀 + base64(JSON{mail,pwd})，读信时解出 pwd
func apihzEncodeToken(mail, pwd string) string {
	raw, _ := json.Marshal(struct {
		Mail string `json:"mail"`
		Pwd  string `json:"pwd"`
	}{Mail: mail, Pwd: pwd})
	return apihzTokPrefix + base64.StdEncoding.EncodeToString(raw)
}

// apihzDecodeToken 去前缀 → base64 解码 → JSON 解析，取出 mail/pwd
func apihzDecodeToken(token string) (mail, pwd string, err error) {
	if !strings.HasPrefix(token, apihzTokPrefix) {
		return "", "", fmt.Errorf("apihz: 无效 token")
	}
	raw, err := base64.StdEncoding.DecodeString(token[len(apihzTokPrefix):])
	if err != nil {
		return "", "", fmt.Errorf("apihz: 无效 token")
	}
	var o struct {
		Mail string `json:"mail"`
		Pwd  string `json:"pwd"`
	}
	if err := json.Unmarshal(raw, &o); err != nil {
		return "", "", fmt.Errorf("apihz: 无效 token")
	}
	mail = strings.TrimSpace(o.Mail)
	pwd = strings.TrimSpace(o.Pwd)
	if mail == "" || pwd == "" {
		return "", "", fmt.Errorf("apihz: 无效 token")
	}
	return mail, pwd, nil
}

type apihzCreateResponse struct {
	Code    int    `json:"code"`
	Msg     string `json:"msg"`
	Mail    string `json:"mail"`
	Pwd     string `json:"pwd"`
	Endtime string `json:"endtime"`
}

type apihzMessage struct {
	Frommail string      `json:"frommail"`
	Fromname string      `json:"fromname"`
	Subject  string      `json:"subject"`
	Time1    json.Number `json:"time1"`
	Time2    string      `json:"time2"`
	Content  string      `json:"content"`
}

type apihzListResponse struct {
	Code int             `json:"code"`
	Msg  string          `json:"msg"`
	Num  json.RawMessage `json:"num"`
	Data []apihzMessage  `json:"data"`
}

// ApihzGenerate 创建 apihz（接口盒子）临时邮箱
// GET /api/mail/mailcache.php?id=&key=&domain=&name=&pwd=&buytype=0
// 有效期 10 分钟，读信必须携带创建时的 pwd
func ApihzGenerate() (*CreatedMailbox, error) {
	id, key := apihzCredentials()
	domain := apihzDomains[rand.Intn(len(apihzDomains))]
	name := apihzRandomLocal(10)
	pwd := apihzRandomPassword()

	u := fmt.Sprintf("%s/api/mail/mailcache.php?id=%s&key=%s&domain=%s&name=%s&pwd=%s&buytype=0",
		apihzBaseURL, url.QueryEscape(id), url.QueryEscape(key),
		url.QueryEscape(domain), url.QueryEscape(name), url.QueryEscape(pwd))

	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("User-Agent", apihzUA)
	req.Header.Set("Accept", "application/json")

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "apihz generate"); err != nil {
		return nil, err
	}

	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var data apihzCreateResponse
	if err := json.Unmarshal(raw, &data); err != nil {
		return nil, fmt.Errorf("apihz: 创建邮箱返回非 JSON: %s", string(raw[:min(len(raw), 120)]))
	}
	if data.Code != 200 || data.Mail == "" {
		msg := data.Msg
		if msg == "" {
			msg = string(raw)
		}
		return nil, fmt.Errorf("apihz: 创建邮箱失败 %s", msg)
	}

	// 优先使用响应回传的 pwd（与请求一致），确保读信密码正确
	finalPwd := data.Pwd
	if finalPwd == "" {
		finalPwd = pwd
	}

	return &CreatedMailbox{
		Channel: "apihz",
		Email:   data.Mail,
		Token:   apihzEncodeToken(data.Mail, finalPwd),
	}, nil
}

// ApihzGetEmails 获取 apihz 邮件列表
// GET /api/mail/mailgetlist.php?id=&key=&mail=&pwd=&page=1
// token 已含 mail 与 pwd，读信只需 token
func ApihzGetEmails(token string) ([]NormEmail, error) {
	if token == "" {
		return nil, fmt.Errorf("apihz: 缺少 token")
	}
	mail, pwd, err := apihzDecodeToken(token)
	if err != nil {
		return nil, err
	}
	id, key := apihzCredentials()

	u := fmt.Sprintf("%s/api/mail/mailgetlist.php?id=%s&key=%s&mail=%s&pwd=%s&page=1",
		apihzBaseURL, url.QueryEscape(id), url.QueryEscape(key),
		url.QueryEscape(mail), url.QueryEscape(pwd))

	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("User-Agent", apihzUA)
	req.Header.Set("Accept", "application/json")

	resp, err := HTTPClient().Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if err := CheckHTTPStatus(resp, "apihz getEmails"); err != nil {
		return nil, err
	}

	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var data apihzListResponse
	if err := json.Unmarshal(raw, &data); err != nil {
		return nil, err
	}
	if data.Code != 200 || len(data.Data) == 0 {
		return []NormEmail{}, nil
	}

	emails := make([]NormEmail, 0, len(data.Data))
	for _, msg := range data.Data {
		from := msg.Frommail
		if from == "" {
			from = msg.Fromname
		}
		date := msg.Time2
		if date == "" {
			date = msg.Time1.String()
		}
		flat := map[string]interface{}{
			"id":      msg.Time1.String(),
			"from":    from,
			"to":      mail,
			"subject": msg.Subject,
			"text":    msg.Content,
			"html":    msg.Content,
			"date":    date,
			"isRead":  false,
		}
		emails = append(emails, NormalizeMap(flat, mail))
	}
	return emails, nil
}
