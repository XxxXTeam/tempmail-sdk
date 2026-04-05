package tempemail

import (
	"os"
	"strconv"
	"strings"
	"sync"
	"time"

	tls_client "github.com/bogdanfinn/tls-client"
)

/*
* SDKConfig SDK 全局配置

*   TEMPMAIL_PROXY    - 代理 URL
*   TEMPMAIL_TIMEOUT  - 超时秒数
*   TEMPMAIL_INSECURE - 设为 "1" 或 "true" 跳过 SSL 验证
*   DROPMAIL_AUTH_TOKEN / DROPMAIL_API_TOKEN - DropMail GraphQL 路径中的 af_ 令牌
*   DROPMAIL_NO_AUTO_TOKEN - 禁止自动拉取/续期
*   DROPMAIL_RENEW_LIFETIME - renew 请求的 lifetime，默认 1d
*   TEMPMAIL_TELEMETRY_ENABLED - true/false，默认 true；设为 false/0/no 关闭匿名用量上报
*   TEMPMAIL_TELEMETRY_URL - 自定义上报端点 URL（覆盖内置默认地址）
 */
type SDKConfig struct {
	/* 代理 URL，支持 http/https/socks5，如 "http://127.0.0.1:7890"，空字符串不使用代理 */
	Proxy string
	/* 全局默认超时，0 使用默认值 15s */
	Timeout time.Duration
	/* 跳过 SSL 证书验证（调试用） */
	Insecure bool
	/* DropMail：GraphQL https://dropmail.me/api/graphql/{token} 的 af_ 令牌，空则自动申请 */
	DropmailAuthToken string
	/* 为 true 时不自动请求令牌（须配置 DropmailAuthToken 或环境变量） */
	DropmailDisableAutoToken bool
	/* 自动续期时传给 /api/token/renew 的 lifetime，如 1d */
	DropmailRenewLifetime string
	/*
	 * 匿名用量上报开关：nil 表示默认开启；指向 false 关闭，指向 true 显式开启
	 */
	TelemetryEnabled *bool
	/* 非空时作为上报服务端 URL，覆盖默认端点与环境变量 TEMPMAIL_TELEMETRY_URL */
	TelemetryEndpoint string
}

var (
	globalConfig  SDKConfig
	configVersion uint64 /* 配置版本号，每次 SetConfig 递增 */
	configMu      sync.RWMutex

	cachedClient            tls_client.HttpClient /* 缓存的 TLS 指纹客户端 */
	cachedNoRedirectClient  tls_client.HttpClient /* 缓存的不跟随重定向客户端 */
	cachedNoCookieJarClient tls_client.HttpClient /* 无 Cookie 罐，供 tempmailg 等需「清空环境」建邮的渠道 */
	cachedClientVersion     uint64                /* 缓存客户端对应的配置版本 */
	clientMu                sync.Mutex

	cachedTenmailWangtzClient tls_client.HttpClient /* 10mail-wangtz：默认跳过证书校验 */
	cachedTenmailWangtzVer    uint64
	tenmailWangtzClientMu     sync.Mutex
)

func init() {
	/* 启动时从环境变量读取默认配置 */
	loadEnvConfig()
}

/*
 * loadEnvConfig 从环境变量读取配置
 * TEMPMAIL_PROXY / TEMPMAIL_TIMEOUT / TEMPMAIL_INSECURE
 */
func loadEnvConfig() {
	if proxy := os.Getenv("TEMPMAIL_PROXY"); proxy != "" {
		globalConfig.Proxy = proxy
	}
	if t := os.Getenv("TEMPMAIL_TIMEOUT"); t != "" {
		if secs, err := strconv.Atoi(t); err == nil && secs > 0 {
			globalConfig.Timeout = time.Duration(secs) * time.Second
		}
	}
	if v := os.Getenv("TEMPMAIL_INSECURE"); v != "" {
		globalConfig.Insecure = v == "1" || strings.EqualFold(v, "true")
	}
	if v := strings.TrimSpace(os.Getenv("DROPMAIL_AUTH_TOKEN")); v != "" {
		globalConfig.DropmailAuthToken = v
	} else if v := strings.TrimSpace(os.Getenv("DROPMAIL_API_TOKEN")); v != "" {
		globalConfig.DropmailAuthToken = v
	}
	if v := strings.TrimSpace(os.Getenv("DROPMAIL_NO_AUTO_TOKEN")); v != "" {
		globalConfig.DropmailDisableAutoToken = v == "1" || strings.EqualFold(v, "true") || strings.EqualFold(v, "yes")
	}
	if v := strings.TrimSpace(os.Getenv("DROPMAIL_RENEW_LIFETIME")); v != "" {
		globalConfig.DropmailRenewLifetime = v
	}
	if v := strings.TrimSpace(os.Getenv("TEMPMAIL_TELEMETRY_ENABLED")); v != "" {
		globalConfig.TelemetryEnabled = parseTelemetryEnabledEnv(v)
	}
	if v := strings.TrimSpace(os.Getenv("TEMPMAIL_TELEMETRY_URL")); v != "" {
		globalConfig.TelemetryEndpoint = v
	}
}

/* parseTelemetryEnabledEnv 解析 true/false；无法识别时返回 nil（保持默认开启） */
func parseTelemetryEnabledEnv(raw string) *bool {
	s := strings.TrimSpace(strings.ToLower(raw))
	f := false
	t := true
	switch s {
	case "false", "0", "no":
		return &f
	case "true", "1", "yes":
		return &t
	default:
		return nil
	}
}

/*
 * SetConfig 设置 SDK 全局配置
 * 线程安全，可在任意时刻调用；设置后自动使已缓存的 HTTP 客户端失效
 *
 * 示例:
 *   tempemail.SetConfig(tempemail.SDKConfig{Insecure: true})
 *   tempemail.SetConfig(tempemail.SDKConfig{Proxy: "http://127.0.0.1:7890"})
 */
func SetConfig(config SDKConfig) {
	configMu.Lock()
	globalConfig = config
	configVersion++
	configMu.Unlock()
	sdkLogger.Info("SDK 配置已更新",
		"proxy", config.Proxy,
		"timeout", config.Timeout.String(),
		"insecure", config.Insecure,
		"telemetry_enabled", telemetryOn(config),
	)
}

/* GetConfig 获取当前 SDK 全局配置 */
func GetConfig() SDKConfig {
	configMu.RLock()
	defer configMu.RUnlock()
	return globalConfig
}

/*
 * buildTLSClient 根据配置和浏览器指纹创建 tls-client 实例
 * @param cfg SDK 全局配置
 * @param bc 浏览器配置（TLS 指纹 profile + UA）
 * @param followRedirect 是否跟随重定向
 * @param withCookieJar 是否启用 Cookie 罐（false 时不持久化 Set-Cookie，适合每次建邮须独立会话的场景）
 * @returns tls_client.HttpClient
 */
func buildTLSClient(cfg SDKConfig, bc BrowserConfig, followRedirect, withCookieJar bool) tls_client.HttpClient {
	timeout := cfg.Timeout
	if timeout <= 0 {
		timeout = 15 * time.Second
	}

	options := []tls_client.HttpClientOption{
		tls_client.WithTimeoutSeconds(int(timeout.Seconds())),
		tls_client.WithClientProfile(bc.Profile),
		tls_client.WithRandomTLSExtensionOrder(),
	}
	if withCookieJar {
		options = append(options, tls_client.WithCookieJar(tls_client.NewCookieJar()))
	}

	if !followRedirect {
		options = append(options, tls_client.WithNotFollowRedirects())
	}

	if cfg.Insecure {
		options = append(options, tls_client.WithInsecureSkipVerify())
	}

	if cfg.Proxy != "" {
		options = append(options, tls_client.WithProxyUrl(cfg.Proxy))
	}

	client, err := tls_client.NewHttpClient(tls_client.NewNoopLogger(), options...)
	if err != nil {
		sdkLogger.Error("创建 TLS 客户端失败，使用默认配置", "error", err.Error())
		client, _ = tls_client.NewHttpClient(tls_client.NewNoopLogger())
	}

	return client
}

/*
 * HTTPClient 获取带全局配置和浏览器 TLS 指纹的 HTTP 客户端
 * 内部缓存复用，仅在配置变更时重建
 * 每次重建时随机选取浏览器配置（profile + UA），模拟真实浏览器指纹
 */
func HTTPClient() tls_client.HttpClient {
	configMu.RLock()
	ver := configVersion
	configMu.RUnlock()

	clientMu.Lock()
	defer clientMu.Unlock()

	/* 缓存命中 */
	if cachedClient != nil && cachedClientVersion == ver {
		return cachedClient
	}

	/* 缓存未命中或配置已变更，重建客户端 */
	cfg := GetConfig()
	bc := RandomBrowserConfig()
	setCurrentBrowser(bc)

	sdkLogger.Debug("创建 TLS 客户端", "ua", bc.UA)

	cachedClient = buildTLSClient(cfg, bc, true, true)
	cachedClientVersion = ver
	cachedNoCookieJarClient = nil /* 主客户端重建时一并失效无罐客户端 */
	return cachedClient
}

/*
 * HTTPClientNoRedirect 获取不跟随重定向的 TLS 客户端
 * 使用当前浏览器配置，用于需要捕获 Set-Cookie 等场景
 * 内部缓存复用，与主客户端同步失效
 */
func HTTPClientNoRedirect() tls_client.HttpClient {
	/* 先确保主客户端已初始化（会设置 currentBrowser） */
	HTTPClient()

	configMu.RLock()
	ver := configVersion
	configMu.RUnlock()

	clientMu.Lock()
	defer clientMu.Unlock()

	if cachedNoRedirectClient != nil && cachedClientVersion == ver {
		return cachedNoRedirectClient
	}

	cfg := GetConfig()
	bc := GetCurrentBrowser()
	cachedNoRedirectClient = buildTLSClient(cfg, bc, false, true)
	return cachedNoRedirectClient
}

/*
 * HTTPClientNoCookieJar 获取与主客户端相同 TLS 指纹与代理配置、但不使用 Cookie 罐的客户端。
 * tempmailg.com 等依赖「首次 GET 下发的会话 Cookie（服务端 Laravel 加密串，常被误称为 JWT）」作为邮箱凭证；
 * 若共用全局 Cookie 罐，第二次 Generate 会带上旧会话，无法换到新邮箱。本客户端仅用于此类渠道。
 */
func HTTPClientNoCookieJar() tls_client.HttpClient {
	HTTPClient()

	configMu.RLock()
	ver := configVersion
	configMu.RUnlock()

	clientMu.Lock()
	defer clientMu.Unlock()

	if cachedNoCookieJarClient != nil && cachedClientVersion == ver {
		return cachedNoCookieJarClient
	}

	cfg := GetConfig()
	bc := GetCurrentBrowser()
	sdkLogger.Debug("创建 TLS 客户端（无 Cookie 罐）", "ua", bc.UA)
	cachedNoCookieJarClient = buildTLSClient(cfg, bc, true, false)
	return cachedNoCookieJarClient
}

/*
 * HTTPClientTenmailWangtz 供 10mail.wangtz.cn 使用：默认跳过 TLS 证书校验（与 curl --insecure 一致），
 * 仍应用当前全局代理与超时。全局 Insecure 为 true 时行为一致。
 */
func HTTPClientTenmailWangtz() tls_client.HttpClient {
	configMu.RLock()
	ver := configVersion
	configMu.RUnlock()

	tenmailWangtzClientMu.Lock()
	defer tenmailWangtzClientMu.Unlock()

	if cachedTenmailWangtzClient != nil && cachedTenmailWangtzVer == ver {
		return cachedTenmailWangtzClient
	}

	cfg := GetConfig()
	cfgCopy := cfg
	cfgCopy.Insecure = true
	bc := GetCurrentBrowser()

	sdkLogger.Debug("创建 TLS 客户端（10mail-wangtz，跳过证书校验）", "ua", bc.UA)
	cachedTenmailWangtzClient = buildTLSClient(cfgCopy, bc, true, true)
	cachedTenmailWangtzVer = ver
	return cachedTenmailWangtzClient
}
