package tempemail

import (
	"crypto/tls"
	"net/http"
	"net/url"
	"os"
	"strconv"
	"strings"
	"sync"
	"time"
)

/*
 * SDKConfig SDK 全局配置
 * 包含代理、超时、SSL 等设置，作用于所有 HTTP 请求
 *
 * 支持环境变量自动读取（优先级低于代码设置）：
 *   TEMPMAIL_PROXY    - 代理 URL
 *   TEMPMAIL_TIMEOUT  - 超时秒数
 *   TEMPMAIL_INSECURE - 设为 "1" 或 "true" 跳过 SSL 验证
 */
type SDKConfig struct {
	/* 代理 URL，支持 http/https/socks5，如 "http://127.0.0.1:7890"，空字符串不使用代理 */
	Proxy string
	/* 全局默认超时，0 使用默认值 15s */
	Timeout time.Duration
	/* 跳过 SSL 证书验证（调试用） */
	Insecure bool
}

var (
	globalConfig  SDKConfig
	configVersion uint64 /* 配置版本号，每次 SetConfig 递增 */
	configMu      sync.RWMutex

	cachedClient        *http.Client /* 缓存的 HTTP 客户端 */
	cachedClientVersion uint64       /* 缓存客户端对应的配置版本 */
	clientMu            sync.Mutex
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
	)
}

/* GetConfig 获取当前 SDK 全局配置 */
func GetConfig() SDKConfig {
	configMu.RLock()
	defer configMu.RUnlock()
	return globalConfig
}

/*
 * HTTPClient 获取带全局配置的 HTTP 客户端
 * 内部缓存复用，仅在配置变更时重建，保证连接池复用以提升性能
 */
func HTTPClient() *http.Client {
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

	timeout := cfg.Timeout
	if timeout <= 0 {
		timeout = 15 * time.Second
	}

	transport := &http.Transport{
		MaxIdleConns:        100,
		MaxIdleConnsPerHost: 10,
		IdleConnTimeout:     90 * time.Second,
	}

	/* 代理 */
	if cfg.Proxy != "" {
		proxyURL, err := url.Parse(cfg.Proxy)
		if err == nil {
			transport.Proxy = http.ProxyURL(proxyURL)
		}
	}

	/* SSL 验证 */
	if cfg.Insecure {
		transport.TLSClientConfig = &tls.Config{InsecureSkipVerify: true}
	}

	cachedClient = &http.Client{
		Timeout:   timeout,
		Transport: transport,
	}
	cachedClientVersion = ver
	return cachedClient
}
