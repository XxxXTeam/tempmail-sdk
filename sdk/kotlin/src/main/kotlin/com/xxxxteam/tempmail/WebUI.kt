package com.xxxxteam.tempmail

import com.sun.net.httpserver.HttpExchange
import com.sun.net.httpserver.HttpServer
import kotlinx.serialization.json.*
import java.io.OutputStream
import java.net.InetSocketAddress
import java.time.LocalTime
import java.time.format.DateTimeFormatter
import java.util.concurrent.CopyOnWriteArrayList
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean

/**
 * 嵌入式 WebUI HTTP 服务器
 *
 * 通过环境变量激活：
 * - TEMPMAIL_WEBUI=true/1/yes 启用
 * - TEMPMAIL_WEBUI_HOST 指定监听地址（默认 127.0.0.1）
 * - TEMPMAIL_WEBUI_PORT 指定端口（未设置则随机分配）
 */
object WebUI {
    private val started = AtomicBoolean(false)
    private var actualPort = 0
    private var actualHost = "127.0.0.1"
    private val sseClients = CopyOnWriteArrayList<OutputStream>()
    private val timeFmt = DateTimeFormatter.ofPattern("HH:mm:ss")

    /** 检查环境变量，命中则启动 */
    fun maybeStartFromEnv() {
        val v = System.getenv("TEMPMAIL_WEBUI") ?: return
        if (v.trim().lowercase() in listOf("true", "1", "yes")) {
            start()
        }
    }

    /** 启动 WebUI（幂等） */
    fun start(host: String? = null, port: Int = 0) {
        if (!started.compareAndSet(false, true)) return

        val bindHost = host?.takeIf { it.isNotBlank() }
            ?: System.getenv("TEMPMAIL_WEBUI_HOST")?.takeIf { it.isNotBlank() }
            ?: "127.0.0.1"

        var bindPort = port
        if (bindPort <= 0) {
            bindPort = System.getenv("TEMPMAIL_WEBUI_PORT")?.trim()?.toIntOrNull() ?: 0
        }

        try {
            val server = HttpServer.create(InetSocketAddress(bindHost, bindPort), 0)
            actualPort = server.address.port
            actualHost = bindHost

            server.createContext("/") { handleIndex(it) }
            server.createContext("/api/info") { handleInfo(it) }
            server.createContext("/api/channels") { handleChannels(it) }
            server.createContext("/api/logs/stream") { handleSSE(it) }
            server.executor = Executors.newCachedThreadPool { r ->
                Thread(r, "tempmail-webui").apply { isDaemon = true }
            }
            server.start()
            log("info", "", "WebUI 已启动 http://$actualHost:$actualPort")
        } catch (e: Exception) {
            started.set(false)
            System.err.println("WebUI 启动失败: ${e.message}")
        }
    }

    /** 向 SSE 客户端推送日志 */
    fun log(level: String, channel: String, msg: String) {
        if (sseClients.isEmpty()) return
        val entry = buildJsonObject {
            put("time", LocalTime.now().format(timeFmt))
            put("level", level.ifBlank { "info" })
            put("msg", msg)
            if (channel.isNotBlank()) put("channel", channel)
        }
        val payload = "data: $entry\n\n".toByteArray()
        for (os in sseClients) {
            try {
                synchronized(os) { os.write(payload); os.flush() }
            } catch (_: Exception) {
                sseClients.remove(os)
            }
        }
    }

    private fun handleIndex(ex: HttpExchange) {
        if (ex.requestURI.path != "/") {
            ex.sendResponseHeaders(404, -1); ex.close(); return
        }
        val body = PAGE_HTML.toByteArray()
        ex.responseHeaders["Content-Type"] = listOf("text/html; charset=utf-8")
        ex.sendResponseHeaders(200, body.size.toLong())
        ex.responseBody.use { it.write(body) }
    }

    private fun handleInfo(ex: HttpExchange) {
        val cfg = Config.get()
        val info = buildJsonObject {
            put("language", "Kotlin")
            put("version", "0.1.0")
            put("host", actualHost)
            put("port", actualPort)
            put("proxy", cfg.proxy ?: "")
            put("timeout", cfg.timeout)
            put("config", buildJsonObject {
                put("telemetry", cfg.telemetryEnabled != false)
                put("insecure", cfg.insecure)
            })
        }
        writeJson(ex, info.toString())
    }

    private fun handleChannels(ex: HttpExchange) {
        val list = buildJsonArray {
            for (spec in Registry.all()) {
                add(buildJsonObject {
                    put("channel", spec.channel)
                    put("name", spec.name)
                    put("website", spec.website)
                })
            }
        }
        writeJson(ex, list.toString())
    }

    private fun handleSSE(ex: HttpExchange) {
        ex.responseHeaders["Content-Type"] = listOf("text/event-stream; charset=utf-8")
        ex.responseHeaders["Cache-Control"] = listOf("no-cache")
        ex.responseHeaders["Connection"] = listOf("keep-alive")
        ex.responseHeaders["Access-Control-Allow-Origin"] = listOf("*")
        ex.sendResponseHeaders(200, 0)

        val os = ex.responseBody
        sseClients.add(os)
        try {
            os.write(": connected\n\n".toByteArray()); os.flush()
            while (true) {
                Thread.sleep(15000)
                synchronized(os) { os.write(": ping\n\n".toByteArray()); os.flush() }
            }
        } catch (_: Exception) {
        } finally {
            sseClients.remove(os)
            ex.close()
        }
    }

    private fun writeJson(ex: HttpExchange, json: String) {
        val body = json.toByteArray()
        ex.responseHeaders["Content-Type"] = listOf("application/json; charset=utf-8")
        ex.sendResponseHeaders(200, body.size.toLong())
        ex.responseBody.use { it.write(body) }
    }

    private const val PAGE_HTML = "<!DOCTYPE html><html lang=\"zh-CN\"><head><meta charset=\"UTF-8\">" +
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0\">" +
        "<title>TempMail SDK - WebUI</title>" +
        "<script src=\"https://cdn.tailwindcss.com\"></script>" +
        "<script src=\"https://unpkg.com/vue@3/dist/vue.global.prod.js\"></script>" +
        "<style>[v-cloak]{display:none}.log-debug{color:#6b7280}.log-info{color:#3b82f6}" +
        ".log-warn{color:#f59e0b}.log-error{color:#ef4444}" +
        "#log-container{max-height:60vh;overflow-y:auto;scroll-behavior:smooth}</style></head>" +
        "<body class=\"bg-gray-900 text-gray-100 min-h-screen\">" +
        "<div id=\"app\" v-cloak>" +
        "<header class=\"bg-gray-800 border-b border-gray-700 px-6 py-4 flex items-center justify-between\">" +
        "<div class=\"flex items-center gap-3\"><h1 class=\"text-xl font-bold\">TempMail SDK</h1>" +
        "<span class=\"text-sm text-gray-400\">{{ sdkInfo.language }} · v{{ sdkInfo.version }}</span></div>" +
        "<div class=\"flex items-center gap-4 text-sm\">" +
        "<span class=\"px-2 py-1 rounded\" :class=\"connected?'bg-green-900 text-green-300':'bg-red-900 text-red-300'\">{{ connected?'已连接':'已断开' }}</span>" +
        "<span class=\"text-gray-400\">{{ sdkInfo.host }}:{{ sdkInfo.port }}</span></div></header>" +
        "<nav class=\"bg-gray-800 border-b border-gray-700 px-6\"><div class=\"flex gap-1\">" +
        "<button v-for=\"tab in tabs\" :key=\"tab.id\" @click=\"activeTab=tab.id\" class=\"px-4 py-2 text-sm rounded-t transition-colors\"" +
        " :class=\"activeTab===tab.id?'bg-gray-900 text-white':'text-gray-400 hover:text-gray-200'\">{{ tab.label }}</button></div></nav>" +
        "<main class=\"p-6\">" +
        "<section v-show=\"activeTab==='overview'\"><div class=\"grid grid-cols-1 md:grid-cols-3 gap-4 mb-6\">" +
        "<div class=\"bg-gray-800 rounded-lg p-4\"><div class=\"text-gray-400 text-sm\">渠道总数</div><div class=\"text-2xl font-bold\">{{ channels.length }}</div></div>" +
        "<div class=\"bg-gray-800 rounded-lg p-4\"><div class=\"text-gray-400 text-sm\">代理</div><div class=\"text-lg font-mono\">{{ sdkInfo.proxy||'未设置' }}</div></div>" +
        "<div class=\"bg-gray-800 rounded-lg p-4\"><div class=\"text-gray-400 text-sm\">超时</div><div class=\"text-lg\">{{ sdkInfo.timeout }}s</div></div></div>" +
        "<div class=\"bg-gray-800 rounded-lg p-4\"><h3 class=\"text-sm text-gray-400 mb-2\">配置</h3>" +
        "<pre class=\"text-xs font-mono text-gray-300 whitespace-pre-wrap\">{{ JSON.stringify(sdkInfo.config,null,2) }}</pre></div></section>" +
        "<section v-show=\"activeTab==='channels'\"><div class=\"mb-4\">" +
        "<input v-model=\"channelSearch\" type=\"text\" placeholder=\"搜索渠道...\" class=\"bg-gray-800 border border-gray-600 rounded px-3 py-2 text-sm w-64 focus:outline-none focus:border-blue-500\"></div>" +
        "<div class=\"bg-gray-800 rounded-lg overflow-hidden\"><table class=\"w-full text-sm\"><thead><tr class=\"border-b border-gray-700\">" +
        "<th class=\"px-4 py-2 text-left text-gray-400\">#</th><th class=\"px-4 py-2 text-left text-gray-400\">渠道标识</th>" +
        "<th class=\"px-4 py-2 text-left text-gray-400\">名称</th><th class=\"px-4 py-2 text-left text-gray-400\">网站</th></tr></thead>" +
        "<tbody><tr v-for=\"(ch,i) in filteredChannels\" :key=\"ch.channel\" class=\"border-b border-gray-700/50 hover:bg-gray-700/30\">" +
        "<td class=\"px-4 py-2 text-gray-500\">{{ i+1 }}</td><td class=\"px-4 py-2 font-mono\">{{ ch.channel }}</td>" +
        "<td class=\"px-4 py-2\">{{ ch.name }}</td><td class=\"px-4 py-2\"><a :href=\"'https://'+ch.website\" target=\"_blank\" class=\"text-blue-400 hover:underline\">{{ ch.website }}</a></td></tr></tbody></table></div></section>" +
        "<section v-show=\"activeTab==='logs'\"><div class=\"flex items-center gap-3 mb-4\">" +
        "<select v-model=\"logLevel\" class=\"bg-gray-800 border border-gray-600 rounded px-2 py-1 text-sm\">" +
        "<option value=\"all\">全部</option><option value=\"debug\">Debug</option><option value=\"info\">Info</option>" +
        "<option value=\"warn\">Warn</option><option value=\"error\">Error</option></select>" +
        "<button @click=\"logs=[]\" class=\"text-sm text-gray-400 hover:text-white\">清空</button>" +
        "<label class=\"text-sm text-gray-400 flex items-center gap-1\"><input type=\"checkbox\" v-model=\"autoScroll\"> 自动滚动</label>" +
        "<span class=\"text-xs text-gray-500 ml-auto\">{{ logs.length }} 条</span></div>" +
        "<div id=\"log-container\" ref=\"logContainer\" class=\"bg-gray-950 rounded-lg p-4 font-mono text-xs\">" +
        "<div v-for=\"(log,i) in filteredLogs\" :key=\"i\" class=\"py-0.5\" :class=\"'log-'+log.level\">" +
        "<span class=\"text-gray-600\">{{ log.time }}</span> <span class=\"px-1\">[{{ log.level.toUpperCase() }}]</span>" +
        "<span v-if=\"log.channel\" class=\"text-purple-400\">[{{ log.channel }}]</span> {{ log.msg }}</div>" +
        "<div v-if=\"filteredLogs.length===0\" class=\"text-gray-600\">暂无日志</div></div></section></main></div>" +
        "<script>const{createApp,ref,computed,onMounted,onUnmounted,nextTick}=Vue;" +
        "createApp({setup(){const tabs=[{id:'overview',label:'概览'},{id:'channels',label:'渠道'},{id:'logs',label:'日志'}];" +
        "const activeTab=ref('overview'),connected=ref(false);" +
        "const sdkInfo=ref({language:'',version:'',host:'127.0.0.1',port:'',proxy:'',timeout:30,config:{}});" +
        "const channels=ref([]),logs=ref([]),channelSearch=ref(''),logLevel=ref('all'),autoScroll=ref(true),logContainer=ref(null);" +
        "let evtSource=null;" +
        "const filteredChannels=computed(()=>{if(!channelSearch.value)return channels.value;const q=channelSearch.value.toLowerCase();return channels.value.filter(c=>c.channel.includes(q)||c.name.toLowerCase().includes(q)||c.website.includes(q))});" +
        "const filteredLogs=computed(()=>logLevel.value==='all'?logs.value:logs.value.filter(l=>l.level===logLevel.value));" +
        "async function fetchInfo(){try{const r=await fetch('/api/info');if(r.ok)sdkInfo.value=await r.json()}catch(e){}}" +
        "async function fetchChannels(){try{const r=await fetch('/api/channels');if(r.ok)channels.value=await r.json()}catch(e){}}" +
        "function connectSSE(){if(evtSource){evtSource.close();evtSource=null}" +
        "evtSource=new EventSource('/api/logs/stream');evtSource.onopen=()=>{connected.value=true};" +
        "evtSource.onmessage=(e)=>{try{const entry=JSON.parse(e.data);logs.value.push(entry);if(logs.value.length>5000)logs.value.splice(0,500);" +
        "if(autoScroll.value)nextTick(()=>{const el=logContainer.value;if(el)el.scrollTop=el.scrollHeight})}catch(err){}};" +
        "evtSource.onerror=()=>{connected.value=false;evtSource.close();evtSource=null;setTimeout(connectSSE,3000)}}" +
        "onMounted(()=>{fetchInfo();fetchChannels();connectSSE()});" +
        "onUnmounted(()=>{if(evtSource)evtSource.close()});" +
        "return{tabs,activeTab,connected,sdkInfo,channels,logs,channelSearch,logLevel,autoScroll,logContainer,filteredChannels,filteredLogs}}}).mount('#app')" +
        "</script></body></html>"
}
