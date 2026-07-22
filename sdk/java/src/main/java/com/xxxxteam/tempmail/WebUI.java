package com.xxxxteam.tempmail;

import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpServer;

import java.io.IOException;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.nio.charset.StandardCharsets;
import java.time.LocalTime;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * WebUI 嵌入式 HTTP 服务器。
 *
 * <p>提供轻量可视化面板，展示 SDK 运行信息、渠道列表与实时日志（SSE）。
 * 通过环境变量 {@code TEMPMAIL_WEBUI=true/1/yes} 激活，默认不启动，不影响 SDK 常规使用。</p>
 *
 * <ul>
 *   <li>{@code TEMPMAIL_WEBUI} —— 设为 {@code 1}/{@code true}/{@code yes} 时自动启动</li>
 *   <li>{@code TEMPMAIL_WEBUI_PORT} —— 指定监听端口，未设置则由系统随机分配</li>
 * </ul>
 */
public final class WebUI {

    /** 启动去重标志，保证幂等。 */
    private static final AtomicBoolean STARTED = new AtomicBoolean(false);

    /** 实际监听端口（随机分配后回填）。 */
    private static volatile int actualPort;

    /** 实际监听主机地址。 */
    private static volatile String actualHost = "127.0.0.1";

    /** 活跃 SSE 客户端输出流集合，写失败时移除。 */
    private static final CopyOnWriteArrayList<OutputStream> SSE_CLIENTS = new CopyOnWriteArrayList<>();

    /** 日志时间格式（时:分:秒）。 */
    private static final DateTimeFormatter TIME_FMT = DateTimeFormatter.ofPattern("HH:mm:ss");

    private WebUI() {
    }

    /**
     * 检查环境变量 {@code TEMPMAIL_WEBUI}，命中则启动 WebUI。
     */
    public static void maybeStartFromEnv() {
        String v = System.getenv("TEMPMAIL_WEBUI");
        if (v == null) {
            return;
        }
        String s = v.trim().toLowerCase();
        if ("true".equals(s) || "1".equals(s) || "yes".equals(s)) {
            start();
        }
    }

    /**
     * 启动嵌入式 WebUI HTTP 服务器（幂等，多次调用只启动一次）。
     * host/port 优先读取 {@code TEMPMAIL_WEBUI_HOST} / {@code TEMPMAIL_WEBUI_PORT}，未设置则默认 127.0.0.1:随机端口。
     */
    public static void start() {
        start(null, 0);
    }

    /**
     * 启动嵌入式 WebUI HTTP 服务器，支持指定 host 和 port。
     *
     * @param host 监听地址，null 时读 TEMPMAIL_WEBUI_HOST，仍为 null 则用 127.0.0.1
     * @param port 监听端口，0 时读 TEMPMAIL_WEBUI_PORT，仍为 0 则随机分配
     */
    public static void start(String host, int port) {
        if (!STARTED.compareAndSet(false, true)) {
            return;
        }
        try {
            if (host == null || host.isBlank()) {
                String h = System.getenv("TEMPMAIL_WEBUI_HOST");
                host = (h != null && !h.isBlank()) ? h.trim() : "127.0.0.1";
            }
            if (port <= 0) {
                String p = System.getenv("TEMPMAIL_WEBUI_PORT");
                if (p != null && !p.isBlank()) {
                    try {
                        port = Integer.parseInt(p.trim());
                    } catch (NumberFormatException ignored) {
                    }
                }
            }
            HttpServer server = HttpServer.create(new InetSocketAddress(host, port), 0);
            actualPort = server.getAddress().getPort();
            actualHost = host;
            server.createContext("/", WebUI::handleIndex);
            server.createContext("/api/info", WebUI::handleInfo);
            server.createContext("/api/channels", WebUI::handleChannels);
            server.createContext("/api/logs/stream", WebUI::handleSSE);
            // 缓存线程池：每个 SSE 长连接独占一个守护线程，避免阻塞默认单线程分发
            server.setExecutor(Executors.newCachedThreadPool(r -> {
                Thread t = new Thread(r, "tempmail-webui-worker");
                t.setDaemon(true);
                return t;
            }));
            server.start();
            log("info", "", "WebUI 已启动 http://" + actualHost + ":" + actualPort);
        } catch (IOException e) {
            STARTED.set(false);
            System.err.println("WebUI 启动失败: " + e.getMessage());
        }
    }

    /**
     * 向所有活跃 SSE 客户端推送一条日志（各 provider 可调用）。
     *
     * @param level   级别：debug/info/warn/error
     * @param channel 渠道标识，可为空串
     * @param msg     日志内容
     */
    public static void log(String level, String channel, String msg) {
        if (SSE_CLIENTS.isEmpty()) {
            return;
        }
        Map<String, Object> entry = new LinkedHashMap<>();
        entry.put("time", LocalTime.now().format(TIME_FMT));
        entry.put("level", level == null || level.isBlank() ? "info" : level);
        entry.put("msg", msg == null ? "" : msg);
        if (channel != null && !channel.isBlank()) {
            entry.put("channel", channel);
        }
        byte[] payload = ("data: " + Json.serialize(entry) + "\n\n").getBytes(StandardCharsets.UTF_8);
        for (OutputStream os : SSE_CLIENTS) {
            try {
                synchronized (os) {
                    os.write(payload);
                    os.flush();
                }
            } catch (IOException e) {
                // 写失败视为客户端断开，移除该连接
                SSE_CLIENTS.remove(os);
            }
        }
    }

    /**
     * 返回前端 HTML 页面。
     *
     * @param ex HTTP 交换
     * @throws IOException 写响应失败
     */
    private static void handleIndex(HttpExchange ex) throws IOException {
        if (!"/".equals(ex.getRequestURI().getPath())) {
            ex.sendResponseHeaders(404, -1);
            ex.close();
            return;
        }
        byte[] body = PAGE_HTML.getBytes(StandardCharsets.UTF_8);
        ex.getResponseHeaders().set("Content-Type", "text/html; charset=utf-8");
        ex.sendResponseHeaders(200, body.length);
        try (OutputStream os = ex.getResponseBody()) {
            os.write(body);
        }
    }

    /**
     * 返回 SDK 运行信息 JSON。
     *
     * @param ex HTTP 交换
     * @throws IOException 写响应失败
     */
    private static void handleInfo(HttpExchange ex) throws IOException {
        SdkConfig cfg = Config.get();
        int timeout = cfg.getTimeout() > 0 ? cfg.getTimeout() : 15;

        Map<String, Object> config = new LinkedHashMap<>();
        // 遥测开关：null 表示默认开启
        config.put("telemetry", cfg.getTelemetryEnabled() == null || cfg.getTelemetryEnabled());
        config.put("insecure", cfg.isInsecure());

        Map<String, Object> info = new LinkedHashMap<>();
        info.put("language", "Java");
        info.put("version", "0.1.0");
        info.put("host", actualHost);
        info.put("port", actualPort);
        info.put("proxy", cfg.getProxy() == null ? "" : cfg.getProxy());
        info.put("timeout", timeout);
        info.put("config", config);

        writeJson(ex, info);
    }

    /**
     * 返回全部渠道列表 JSON。
     *
     * @param ex HTTP 交换
     * @throws IOException 写响应失败
     */
    private static void handleChannels(HttpExchange ex) throws IOException {
        List<Map<String, String>> list = new ArrayList<>();
        for (ChannelSpec spec : Registry.all()) {
            Map<String, String> item = new LinkedHashMap<>();
            item.put("channel", spec.getChannel());
            item.put("name", spec.getName());
            item.put("website", spec.getWebsite());
            list.add(item);
        }
        writeJson(ex, list);
    }

    /**
     * 建立 SSE 长连接，实时推送日志。
     *
     * @param ex HTTP 交换
     * @throws IOException 建立连接失败
     */
    private static void handleSSE(HttpExchange ex) throws IOException {
        ex.getResponseHeaders().set("Content-Type", "text/event-stream; charset=utf-8");
        ex.getResponseHeaders().set("Cache-Control", "no-cache");
        ex.getResponseHeaders().set("Connection", "keep-alive");
        ex.getResponseHeaders().set("Access-Control-Allow-Origin", "*");
        ex.sendResponseHeaders(200, 0);

        OutputStream os = ex.getResponseBody();
        SSE_CLIENTS.add(os);
        try {
            // 首帧注释，触发连接建立
            os.write(": connected\n\n".getBytes(StandardCharsets.UTF_8));
            os.flush();
            // 心跳保活，直到写失败（客户端断开）
            while (true) {
                Thread.sleep(15000);
                synchronized (os) {
                    os.write(": ping\n\n".getBytes(StandardCharsets.UTF_8));
                    os.flush();
                }
            }
        } catch (IOException | InterruptedException e) {
            // 连接断开，退出循环
        } finally {
            SSE_CLIENTS.remove(os);
            ex.close();
        }
    }

    /**
     * 序列化对象为 JSON 并写回响应。
     *
     * @param ex    HTTP 交换
     * @param value 待序列化对象
     * @throws IOException 写响应失败
     */
    private static void writeJson(HttpExchange ex, Object value) throws IOException {
        byte[] body = Json.serialize(value).getBytes(StandardCharsets.UTF_8);
        ex.getResponseHeaders().set("Content-Type", "application/json; charset=utf-8");
        ex.sendResponseHeaders(200, body.length);
        try (OutputStream os = ex.getResponseBody()) {
            os.write(body);
        }
    }

    /** 前端完整 HTML 页面（index.html 内联 app.js.html 脚本）。 */
    private static final String PAGE_HTML = ""
        + """
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>TempMail SDK - WebUI</title>
<script src="https://cdn.tailwindcss.com"></script>
<script src="https://unpkg.com/vue@3/dist/vue.global.prod.js"></script>
<style>
[v-cloak]{display:none}
.log-debug{color:#6b7280}.log-info{color:#3b82f6}.log-warn{color:#f59e0b}.log-error{color:#ef4444}
.fade-enter-active{transition:opacity .3s}.fade-enter-from{opacity:0}
#log-container{max-height:60vh;overflow-y:auto;scroll-behavior:smooth}
</style>
</head>
<body class="bg-gray-900 text-gray-100 min-h-screen">
<div id="app" v-cloak>
  <header class="bg-gray-800 border-b border-gray-700 px-6 py-4 flex items-center justify-between">
    <div class="flex items-center gap-3">
      <h1 class="text-xl font-bold">TempMail SDK</h1>
      <span class="text-sm text-gray-400">{{ sdkInfo.language }} · v{{ sdkInfo.version }}</span>
    </div>
    <div class="flex items-center gap-4 text-sm">
      <span class="px-2 py-1 rounded" :class="connected ? 'bg-green-900 text-green-300' : 'bg-red-900 text-red-300'">
        {{ connected ? '已连接' : '已断开' }}
      </span>
      <span class="text-gray-400">端口 {{ sdkInfo.port }}</span>
    </div>
  </header>

  <nav class="bg-gray-800 border-b border-gray-700 px-6">
    <div class="flex gap-1">
      <button v-for="tab in tabs" :key="tab.id" @click="activeTab=tab.id"
        class="px-4 py-2 text-sm rounded-t transition-colors"
        :class="activeTab===tab.id ? 'bg-gray-900 text-white' : 'text-gray-400 hover:text-gray-200'">
        {{ tab.label }}
      </button>
    </div>
  </nav>
"""
        + """
  <main class="p-6">
    <!-- 概览 -->
    <section v-show="activeTab==='overview'">
      <div class="grid grid-cols-1 md:grid-cols-3 gap-4 mb-6">
        <div class="bg-gray-800 rounded-lg p-4">
          <div class="text-gray-400 text-sm">渠道总数</div>
          <div class="text-2xl font-bold">{{ channels.length }}</div>
        </div>
        <div class="bg-gray-800 rounded-lg p-4">
          <div class="text-gray-400 text-sm">代理</div>
          <div class="text-lg font-mono">{{ sdkInfo.proxy || '未设置' }}</div>
        </div>
        <div class="bg-gray-800 rounded-lg p-4">
          <div class="text-gray-400 text-sm">超时</div>
          <div class="text-lg">{{ sdkInfo.timeout }}s</div>
        </div>
      </div>
      <div class="bg-gray-800 rounded-lg p-4">
        <h3 class="text-sm text-gray-400 mb-2">配置</h3>
        <pre class="text-xs font-mono text-gray-300 whitespace-pre-wrap">{{ JSON.stringify(sdkInfo.config, null, 2) }}</pre>
      </div>
    </section>

    <!-- 渠道 -->
    <section v-show="activeTab==='channels'">
      <div class="mb-4">
        <input v-model="channelSearch" type="text" placeholder="搜索渠道..."
          class="bg-gray-800 border border-gray-600 rounded px-3 py-2 text-sm w-64 focus:outline-none focus:border-blue-500">
      </div>
      <div class="bg-gray-800 rounded-lg overflow-hidden">
        <table class="w-full text-sm">
          <thead class="bg-gray-750">
            <tr class="border-b border-gray-700">
              <th class="px-4 py-2 text-left text-gray-400">#</th>
              <th class="px-4 py-2 text-left text-gray-400">渠道标识</th>
              <th class="px-4 py-2 text-left text-gray-400">名称</th>
              <th class="px-4 py-2 text-left text-gray-400">网站</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="(ch, i) in filteredChannels" :key="ch.channel"
              class="border-b border-gray-700/50 hover:bg-gray-700/30">
              <td class="px-4 py-2 text-gray-500">{{ i + 1 }}</td>
              <td class="px-4 py-2 font-mono">{{ ch.channel }}</td>
              <td class="px-4 py-2">{{ ch.name }}</td>
              <td class="px-4 py-2"><a :href="'https://'+ch.website" target="_blank" class="text-blue-400 hover:underline">{{ ch.website }}</a></td>
            </tr>
          </tbody>
        </table>
      </div>
    </section>
"""
        + """
    <!-- 日志 -->
    <section v-show="activeTab==='logs'">
      <div class="flex items-center gap-3 mb-4">
        <select v-model="logLevel" class="bg-gray-800 border border-gray-600 rounded px-2 py-1 text-sm">
          <option value="all">全部</option>
          <option value="debug">Debug</option>
          <option value="info">Info</option>
          <option value="warn">Warn</option>
          <option value="error">Error</option>
        </select>
        <button @click="logs=[]" class="text-sm text-gray-400 hover:text-white">清空</button>
        <label class="text-sm text-gray-400 flex items-center gap-1">
          <input type="checkbox" v-model="autoScroll"> 自动滚动
        </label>
        <span class="text-xs text-gray-500 ml-auto">{{ logs.length }} 条</span>
      </div>
      <div id="log-container" ref="logContainer" class="bg-gray-950 rounded-lg p-4 font-mono text-xs">
        <div v-for="(log, i) in filteredLogs" :key="i" class="py-0.5" :class="'log-'+log.level">
          <span class="text-gray-600">{{ log.time }}</span>
          <span class="px-1">[{{ log.level.toUpperCase() }}]</span>
          <span v-if="log.channel" class="text-purple-400">[{{ log.channel }}]</span>
          {{ log.msg }}
        </div>
        <div v-if="filteredLogs.length===0" class="text-gray-600">暂无日志</div>
      </div>
    </section>
  </main>
</div>
"""
        + """
<script>
const {createApp, ref, computed, onMounted, onUnmounted, nextTick, watch} = Vue
createApp({
  setup() {
    const tabs = [{id:'overview',label:'概览'},{id:'channels',label:'渠道'},{id:'logs',label:'日志'}]
    const activeTab = ref('overview')
    const connected = ref(false)
    const sdkInfo = ref({language:'',version:'',port:'',proxy:'',timeout:30,config:{}})
    const channels = ref([])
    const logs = ref([])
    const channelSearch = ref('')
    const logLevel = ref('all')
    const autoScroll = ref(true)
    const logContainer = ref(null)
    let evtSource = null

    const filteredChannels = computed(() => {
      if (!channelSearch.value) return channels.value
      const q = channelSearch.value.toLowerCase()
      return channels.value.filter(c =>
        c.channel.includes(q) || c.name.toLowerCase().includes(q) || c.website.includes(q))
    })
    const filteredLogs = computed(() => {
      if (logLevel.value === 'all') return logs.value
      return logs.value.filter(l => l.level === logLevel.value)
    })

    async function fetchInfo() {
      try {
        const r = await fetch('/api/info')
        if (r.ok) sdkInfo.value = await r.json()
      } catch(e) {}
    }
    async function fetchChannels() {
      try {
        const r = await fetch('/api/channels')
        if (r.ok) channels.value = await r.json()
      } catch(e) {}
    }
    function connectSSE() {
      evtSource = new EventSource('/api/logs/stream')
      evtSource.onopen = () => { connected.value = true }
      evtSource.onmessage = (e) => {
        try {
          const entry = JSON.parse(e.data)
          logs.value.push(entry)
          if (logs.value.length > 5000) logs.value.splice(0, 500)
          if (autoScroll.value) {
            nextTick(() => {
              const el = logContainer.value
              if (el) el.scrollTop = el.scrollHeight
            })
          }
        } catch(err) {}
      }
      evtSource.onerror = () => {
        connected.value = false
        setTimeout(connectSSE, 3000)
      }
    }

    onMounted(() => {
      fetchInfo()
      fetchChannels()
      connectSSE()
    })
    onUnmounted(() => { if (evtSource) evtSource.close() })

    return {tabs,activeTab,connected,sdkInfo,channels,logs,channelSearch,logLevel,
            autoScroll,logContainer,filteredChannels,filteredLogs}
  }
}).mount('#app')
</script>
</body>
</html>
"""
        ;
}
