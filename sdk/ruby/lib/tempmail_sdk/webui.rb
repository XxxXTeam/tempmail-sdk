# frozen_string_literal: true

require "socket"
require "thread"
require "json"

module TempmailSdk
  # 嵌入式 WebUI HTTP 服务器
  # 通过 TEMPMAIL_WEBUI=true/1/yes 激活，TEMPMAIL_WEBUI_PORT 指定端口（未设置则随机）
  # 仅依赖 Ruby 标准库，不引入任何外部 gem
  module WebUI
    module_function

    # SSE 客户端队列列表与互斥锁
    @sse_clients = []
    @sse_mutex   = Mutex.new
    @server_mutex = Mutex.new
    @server      = nil
    @port        = nil

    class << self
      attr_reader :port, :host

      # 启动 WebUI（幂等）
      def start(host: nil, port: nil)
        @server_mutex.synchronize do
          return @port if @server

          bind_host = host || _resolve_host
          bind_port = port || _resolve_port
          @server = TCPServer.new(bind_host, bind_port)
          @host   = bind_host
          @port   = @server.addr[1]

          # 挂接日志处理器
          _attach_log_handler

          Thread.new { _serve }
          Log.logger.info("WebUI 已启动: http://#{@host}:#{@port}")
          @port
        end
      end

      # 广播日志条目到所有 SSE 连接
      def broadcast(entry)
        data = JSON.generate(entry)
        @sse_mutex.synchronize { @sse_clients.dup }.each do |q|
          q.push(data) rescue nil
        end
      end

      private

      def _resolve_host
        raw = (ENV["TEMPMAIL_WEBUI_HOST"] || "").strip
        raw.empty? ? "127.0.0.1" : raw
      end

      def _resolve_port
        raw = (ENV["TEMPMAIL_WEBUI_PORT"] || "").strip
        raw.match?(/\A\d+\z/) ? raw.to_i : 0
      end

      def _attach_log_handler
        orig_formatter = Log.logger.formatter
        Log.logger.formatter = proc do |severity, time, progname, msg|
          entry = {
            time:    time.strftime("%H:%M:%S"),
            level:   _severity_to_level(severity),
            channel: nil,
            msg:     msg.to_s
          }
          broadcast(entry)
          orig_formatter ? orig_formatter.call(severity, time, progname, msg) : "#{severity} #{msg}\n"
        end
        # 确保 logger 级别足够低以捕获 debug
        Log.logger.level = Logger::DEBUG if Log.logger.level > Logger::DEBUG
      end

      def _severity_to_level(sev)
        case sev.to_s.upcase
        when "ERROR", "FATAL" then "error"
        when "WARN"           then "warn"
        when "INFO"           then "info"
        else                       "debug"
        end
      end

      def _serve
        loop do
          client = @server.accept rescue break
          Thread.new(client) { |c| _handle(c) }
        end
      end

      def _handle(socket)
        line = socket.gets&.chomp || ""
        method, path, = line.split(" ")
        # 消费请求头
        loop do
          h = socket.gets
          break if h.nil? || h.chomp.empty?
        end

        path = (path || "/").split("?", 2).first

        case [method, path]
        when ["GET", "/"]
          _send_html(socket, _rendered_html)
        when ["GET", "/api/info"]
          _send_json(socket, _build_info)
        when ["GET", "/api/channels"]
          _send_json(socket, _build_channels)
        when ["GET", "/api/logs/stream"]
          _handle_sse(socket)
        else
          socket.write "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n"
        end
      rescue StandardError
        # 忽略单连接异常，不影响服务器主循环
      ensure
        socket.close rescue nil
      end

      def _send_html(socket, html)
        body = html.encode("UTF-8")
        socket.write "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n" \
                     "Content-Length: #{body.bytesize}\r\nConnection: close\r\n\r\n#{body}"
      end

      def _send_json(socket, payload)
        body = JSON.generate(payload)
        socket.write "HTTP/1.1 200 OK\r\nContent-Type: application/json; charset=utf-8\r\n" \
                     "Content-Length: #{body.bytesize}\r\nConnection: close\r\n\r\n#{body}"
      end

      def _handle_sse(socket)
        q = Queue.new
        @sse_mutex.synchronize { @sse_clients << q }
        socket.write "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream; charset=utf-8\r\n" \
                     "Cache-Control: no-cache\r\nConnection: keep-alive\r\nX-Accel-Buffering: no\r\n\r\n"
        socket.write ": connected\n\n"
        socket.flush
        loop do
          data = q.pop
          socket.write "data: #{data}\n\n"
          socket.flush
        end
      rescue StandardError
        # 客户端断开
      ensure
        @sse_mutex.synchronize { @sse_clients.delete(q) }
      end

      def _build_info
        cfg = Config.get_config
        te  = cfg.telemetry_enabled
        te  = true if te.nil?
        {
          language: "Ruby",
          version:  VERSION,
          host:     @host,
          port:     @port,
          proxy:    cfg.proxy || "",
          timeout:  cfg.timeout,
          config:   { telemetry: te == true, insecure: cfg.insecure == true }
        }
      end

      def _build_channels
        Registry::REGISTRY.map { |s| { channel: s.channel, name: s.name, website: s.website } }
      end

      def _rendered_html
        @_rendered_html ||= HTML_TEMPLATE.sub("<!-- PLACEHOLDER_SCRIPT -->", APP_SCRIPT)
      end
    end

    # 自动检查环境变量并启动
    def auto_start
      flag = (ENV["TEMPMAIL_WEBUI"] || "").strip.downcase
      WebUI.start if %w[1 true yes on].include?(flag)
    end

    # rubocop:disable Layout/LineLength
    HTML_TEMPLATE = <<~'HTML'
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
        <main class="p-6">
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
      <!-- PLACEHOLDER_SCRIPT -->
      </body>
      </html>
    HTML

    APP_SCRIPT = <<~'JS'
      <script>
      const {createApp, ref, computed, onMounted, onUnmounted, nextTick} = Vue
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
            try { const r = await fetch('/api/info'); if (r.ok) sdkInfo.value = await r.json() } catch(e) {}
          }
          async function fetchChannels() {
            try { const r = await fetch('/api/channels'); if (r.ok) channels.value = await r.json() } catch(e) {}
          }
          function connectSSE() {
            evtSource = new EventSource('/api/logs/stream')
            evtSource.onopen = () => { connected.value = true }
            evtSource.onmessage = (e) => {
              try {
                const entry = JSON.parse(e.data)
                logs.value.push(entry)
                if (logs.value.length > 5000) logs.value.splice(0, 500)
                if (autoScroll.value) nextTick(() => { const el = logContainer.value; if (el) el.scrollTop = el.scrollHeight })
              } catch(err) {}
            }
            evtSource.onerror = () => { connected.value = false; setTimeout(connectSSE, 3000) }
          }
          onMounted(() => { fetchInfo(); fetchChannels(); connectSSE() })
          onUnmounted(() => { if (evtSource) evtSource.close() })
          return {tabs,activeTab,connected,sdkInfo,channels,logs,channelSearch,logLevel,
                  autoScroll,logContainer,filteredChannels,filteredLogs}
        }
      }).mount('#app')
      </script>
    JS
    # rubocop:enable Layout/LineLength
  end
end
