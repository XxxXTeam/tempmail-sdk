package tempemail

import (
	"context"
	"encoding/json"
	"fmt"
	"log/slog"
	"net"
	"net/http"
	"os"
	"strings"
	"sync"
	"time"
)

/*
 * WebUI 嵌入式 HTTP 服务器
 * 提供一个轻量的可视化面板，用于查看 SDK 运行信息、渠道列表与实时日志（SSE）。
 * 通过环境变量 TEMPMAIL_WEBUI=true 激活，默认不启动，不影响 SDK 常规使用。
 *
 * 环境变量:
 *   TEMPMAIL_WEBUI       - 设为 1/true/yes 时自动启动 WebUI
 *   TEMPMAIL_WEBUI_HOST  - 指定监听 host，未设置则默认 127.0.0.1
 *   TEMPMAIL_WEBUI_PORT  - 指定监听端口，未设置则由系统随机分配
 */

/* webuiVersion WebUI 面板展示的 SDK 版本号 */
const webuiVersion = "1.0.0"

/* webuiPage 完整前端页面（index.html 已内联 app.js.html 脚本） */
const webuiPage = webuiIndexHTML

/* webuiState WebUI 运行时状态（实际端口 + host + 启动去重） */
var (
	webuiActualPort int
	webuiActualHost string
	webuiStartOnce  sync.Once
	webuiStateMu    sync.RWMutex
)

/* webuiLogEntry SSE 推送的单条日志结构 */
type webuiLogEntry struct {
	Time    string `json:"time"`
	Level   string `json:"level"`
	Msg     string `json:"msg"`
	Channel string `json:"channel,omitempty"`
}

/*
 * webuiBroadcaster SSE 广播器，管理所有活跃客户端连接
 * goroutine 安全，支持多客户端并发订阅
 */
type webuiBroadcaster struct {
	mu      sync.RWMutex
	clients map[chan webuiLogEntry]struct{}
}

var webuiBcast = &webuiBroadcaster{
	clients: make(map[chan webuiLogEntry]struct{}),
}

/* subscribe 注册一个新的 SSE 客户端通道 */
func (b *webuiBroadcaster) subscribe() chan webuiLogEntry {
	ch := make(chan webuiLogEntry, 64)
	b.mu.Lock()
	b.clients[ch] = struct{}{}
	b.mu.Unlock()
	return ch
}

/* unsubscribe 注销并关闭客户端通道 */
func (b *webuiBroadcaster) unsubscribe(ch chan webuiLogEntry) {
	b.mu.Lock()
	delete(b.clients, ch)
	b.mu.Unlock()
	close(ch)
}

/* broadcast 向所有客户端广播一条日志，通道满时丢弃（非阻塞） */
func (b *webuiBroadcaster) broadcast(entry webuiLogEntry) {
	b.mu.RLock()
	defer b.mu.RUnlock()
	for ch := range b.clients {
		select {
		case ch <- entry:
		default:
		}
	}
}

/*
 * WebUIHandler 自定义 slog.Handler，tee 模式：
 * 将每条日志同时转发给原始 handler 和 SSE 广播器
 */
type WebUIHandler struct {
	inner slog.Handler
}

/* Enabled 委托给内层 handler */
func (h *WebUIHandler) Enabled(ctx context.Context, level slog.Level) bool {
	return true /* WebUI 捕获所有级别 */
}

/* Handle 转发日志到内层 handler 并广播到 SSE */
func (h *WebUIHandler) Handle(ctx context.Context, r slog.Record) error {
	/* 提取 channel 属性 */
	var channel string
	r.Attrs(func(a slog.Attr) bool {
		if a.Key == "channel" {
			channel = a.Value.String()
			return false
		}
		return true
	})

	entry := webuiLogEntry{
		Time:    r.Time.Format(time.RFC3339),
		Level:   strings.ToLower(r.Level.String()),
		Msg:     r.Message,
		Channel: channel,
	}
	webuiBcast.broadcast(entry)

	if h.inner.Enabled(ctx, r.Level) {
		return h.inner.Handle(ctx, r)
	}
	return nil
}

/* WithAttrs 委托给内层 handler */
func (h *WebUIHandler) WithAttrs(attrs []slog.Attr) slog.Handler {
	return &WebUIHandler{inner: h.inner.WithAttrs(attrs)}
}

/* WithGroup 委托给内层 handler */
func (h *WebUIHandler) WithGroup(name string) slog.Handler {
	return &WebUIHandler{inner: h.inner.WithGroup(name)}
}

/* wrapLoggerForWebUI 将当前 sdkLogger 包装为 WebUIHandler tee 模式 */
func wrapLoggerForWebUI() {
	handler := &WebUIHandler{inner: sdkLogger.Handler()}
	sdkLogger = slog.New(handler).With("module", "tempmail-sdk")
}

/*
 * StartWebUI 启动嵌入式 WebUI HTTP 服务器（幂等，多次调用只启动一次）
 * host 优先读取 TEMPMAIL_WEBUI_HOST（默认 127.0.0.1），端口优先读取 TEMPMAIL_WEBUI_PORT（未设置则随机分配）
 */
func StartWebUI() {
	StartWebUIOn("", "")
}

/*
 * StartWebUIOn 以指定 host 和 port 启动 WebUI（空字符串则读取环境变量或使用默认值）
 * host 默认 127.0.0.1，port 默认随机分配
 */
func StartWebUIOn(host, port string) {
	webuiStartOnce.Do(func() {
		wrapLoggerForWebUI()

		if host == "" {
			host = os.Getenv("TEMPMAIL_WEBUI_HOST")
		}
		if host == "" {
			host = "127.0.0.1"
		}
		if port == "" {
			port = os.Getenv("TEMPMAIL_WEBUI_PORT")
		}
		addr := host + ":" + port

		ln, err := net.Listen("tcp", addr)
		if err != nil {
			sdkLogger.Error("WebUI 监听失败", "error", err.Error())
			return
		}

		webuiStateMu.Lock()
		webuiActualPort = ln.Addr().(*net.TCPAddr).Port
		webuiActualHost = host
		webuiStateMu.Unlock()

		mux := http.NewServeMux()
		mux.HandleFunc("/", webuiHandleIndex)
		mux.HandleFunc("/api/info", webuiHandleInfo)
		mux.HandleFunc("/api/channels", webuiHandleChannels)
		mux.HandleFunc("/api/logs/stream", webuiHandleSSE)

		sdkLogger.Info("WebUI 已启动", "host", host, "port", webuiActualPort)
		go func() {
			if err := http.Serve(ln, mux); err != nil {
				sdkLogger.Error("WebUI 服务退出", "error", err.Error())
			}
		}()
	})
}

func init() {
	if v := os.Getenv("TEMPMAIL_WEBUI"); v == "1" || strings.EqualFold(v, "true") || strings.EqualFold(v, "yes") {
		StartWebUI()
	}
}

/* webuiHandleIndex 返回前端 HTML 页面 */
func webuiHandleIndex(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	fmt.Fprint(w, webuiPage)
}

/* webuiHandleInfo 返回 SDK 运行信息 JSON */
func webuiHandleInfo(w http.ResponseWriter, r *http.Request) {
	cfg := GetConfig()
	webuiStateMu.RLock()
	port := webuiActualPort
	host := webuiActualHost
	webuiStateMu.RUnlock()
	if host == "" {
		host = "127.0.0.1"
	}

	timeout := int(cfg.Timeout.Seconds())
	if timeout <= 0 {
		timeout = 15
	}

	info := map[string]any{
		"language": "Go",
		"version":  webuiVersion,
		"host":     host,
		"port":     port,
		"proxy":    cfg.Proxy,
		"timeout":  timeout,
		"config": map[string]any{
			"telemetry": telemetryOn(cfg),
			"insecure":  cfg.Insecure,
		},
	}
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(info)
}

/* webuiHandleChannels 返回所有渠道列表 JSON */
func webuiHandleChannels(w http.ResponseWriter, r *http.Request) {
	channels := ListChannels()
	list := make([]map[string]string, len(channels))
	for i, ch := range channels {
		list[i] = map[string]string{
			"channel": string(ch.Channel),
			"name":    ch.Name,
			"website": ch.Website,
		}
	}
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(list)
}

/* webuiHandleSSE 建立 SSE 长连接，实时推送日志 */
func webuiHandleSSE(w http.ResponseWriter, r *http.Request) {
	flusher, ok := w.(http.Flusher)
	if !ok {
		http.Error(w, "不支持 SSE", http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", "text/event-stream")
	w.Header().Set("Cache-Control", "no-cache")
	w.Header().Set("Connection", "keep-alive")
	w.Header().Set("Access-Control-Allow-Origin", "*")

	ch := webuiBcast.subscribe()
	defer webuiBcast.unsubscribe(ch)

	ctx := r.Context()
	for {
		select {
		case <-ctx.Done():
			return
		case entry, ok := <-ch:
			if !ok {
				return
			}
			data, err := json.Marshal(entry)
			if err != nil {
				continue
			}
			fmt.Fprintf(w, "data: %s\n\n", data)
			flusher.Flush()
		}
	}
}
