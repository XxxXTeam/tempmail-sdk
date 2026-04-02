package provider

import (
	"crypto/rand"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"math/big"
)

type linshiProfileSer struct {
	ColorDepth          int             `json:"colorDepth"`
	DeviceMemory        int             `json:"deviceMemory"`
	HardwareConcurrency int             `json:"hardwareConcurrency"`
	Language            string          `json:"language"`
	Languages           string          `json:"languages"`
	PixelRatio          float64         `json:"pixelRatio"`
	Platform            string          `json:"platform"`
	Screen              linshiScreenSer `json:"screen"`
	Timezone            string          `json:"timezone"`
	UserAgent           string          `json:"userAgent"`
}

type linshiScreenSer struct {
	Width       int `json:"width"`
	Height      int `json:"height"`
	AvailWidth  int `json:"availWidth"`
	AvailHeight int `json:"availHeight"`
}

var (
	linshiUA = []string{
		"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
		"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/121.0.0.0 Safari/537.36 Edg/121.0.0.0",
		"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
		"Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/119.0.0.0 Safari/537.36",
		"Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:122.0) Gecko/20100101 Firefox/122.0",
	}
	linshiPlatform = []string{"Win32", "MacIntel", "Linux x86_64"}
	linshiLang     = []string{"zh-CN", "en-US", "en-GB", "ja-JP"}
	linshiLangs    = []string{"zh-CN,zh,en", "en-US,en", "ja-JP,ja,en"}
	linshiTZ       = []string{"Asia/Shanghai", "America/New_York", "Europe/London", "UTC"}
	linshiMem      = []int{4, 8, 16}
	linshiDepth    = []int{24, 30}
	linshiRatio    = []float64{1, 1.25, 1.5, 2}
	linshiW        = []int{1920, 2560, 1680, 1536, 1366}
	linshiH        = []int{1080, 1440, 1050, 864, 768}
)

func linshiRandIntn(n int) int {
	if n <= 0 {
		return 0
	}
	v, _ := rand.Int(rand.Reader, big.NewInt(int64(n)))
	return int(v.Int64())
}

func linshiPickStr(a []string) string {
	return a[linshiRandIntn(len(a))]
}

func linshiPickInt(a []int) int {
	return a[linshiRandIntn(len(a))]
}

func linshiPickFloat(a []float64) float64 {
	return a[linshiRandIntn(len(a))]
}

func randomLinshiProfileSer() linshiProfileSer {
	w := linshiPickInt(linshiW) + linshiRandIntn(81)
	h := linshiPickInt(linshiH) + linshiRandIntn(41)
	availH := h - 24 - linshiRandIntn(97)
	if availH < 0 {
		availH = h
	}
	return linshiProfileSer{
		ColorDepth:          linshiPickInt(linshiDepth),
		DeviceMemory:        linshiPickInt(linshiMem),
		HardwareConcurrency: 4 + linshiRandIntn(29),
		Language:            linshiPickStr(linshiLang),
		Languages:           linshiPickStr(linshiLangs),
		PixelRatio:          linshiPickFloat(linshiRatio),
		Platform:            linshiPickStr(linshiPlatform),
		Screen: linshiScreenSer{
			Width:       w,
			Height:      h,
			AvailWidth:  w,
			AvailHeight: availH,
		},
		Timezone:  linshiPickStr(linshiTZ),
		UserAgent: linshiPickStr(linshiUA),
	}
}

// DeriveLinshiAPIPathKey 与 linshi 前端 u(visitorId) 等价
func DeriveLinshiAPIPathKey(visitorID string) string {
	if len(visitorID) < 4 {
		panic("visitorId too short")
	}
	e := visitorID[:len(visitorID)-2]
	t := 0
	for i := 0; i < len(e); i++ {
		t += int(e[i]) % 5
	}
	if t < 10 {
		t = 10
	}
	if t >= 100 {
		t = 99
	}
	ts0 := byte('0' + t/10)
	ts1 := byte('0' + t%10)
	e = e[:3] + string(ts0) + e[3:]
	e = e[:12] + string(ts1) + e[12:]
	return e
}

// syntheticVisitorIDFromProfileSer SHA256(JSON+salt) 前 32 hex，JSON 与 TS 一致
func syntheticVisitorIDFromProfileSer(p linshiProfileSer, salt []byte) string {
	if salt == nil {
		salt = make([]byte, 16)
		_, _ = rand.Read(salt)
	}
	payload, _ := json.Marshal(p)
	h := sha256.New()
	h.Write(payload)
	h.Write(salt)
	sum := h.Sum(nil)
	return hex.EncodeToString(sum)[:32]
}

// RandomSyntheticLinshiAPIPathKey 随机画像 → visitorId → REST path key
func RandomSyntheticLinshiAPIPathKey() string {
	prof := randomLinshiProfileSer()
	vid := syntheticVisitorIDFromProfileSer(prof, nil)
	return DeriveLinshiAPIPathKey(vid)
}
