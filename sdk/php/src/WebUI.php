<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail;

/**
 * 嵌入式 WebUI HTTP 服务器
 *
 * 通过环境变量 TEMPMAIL_WEBUI=true/1/yes 激活。
 * 端口由 TEMPMAIL_WEBUI_PORT 指定，未设置则随机绑定（bind port 0）。
 * 使用 PHP 内置 stream_socket_server，无外部依赖。
 * 日志通过 Logger::setWebUISink() 写入临时文件，SSE 端点 tail 该文件推送。
 */
final class WebUI
{
    private static bool $started = false;

    /** 前端 HTML（index.html + app.js.html 内联） */
    private const HTML = <<<'HTMLEOF'
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
          <thead>
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
</body>
</html>
HTMLEOF;

    /**
     * 检查环境变量，若启用则启动 WebUI 服务器。
     * 优先使用 pcntl_fork 在子进程运行；不可用时用 proc_open 启动独立 PHP 进程。
     */
    public static function maybeStart(): void
    {
        if (self::$started) {
            return;
        }
        $enabled = strtolower(trim((string)(getenv('TEMPMAIL_WEBUI') ?: '')));
        if (!in_array($enabled, ['true', '1', 'yes'], true)) {
            return;
        }
        self::$started = true;

        $port = (int)(getenv('TEMPMAIL_WEBUI_PORT') ?: 0);
        $host = trim((string)(getenv('TEMPMAIL_WEBUI_HOST') ?: ''));
        if ($host === '') {
            $host = '127.0.0.1';
        }
        $sinkFile = sys_get_temp_dir() . '/tempmail_webui_' . getmypid() . '.log';
        Logger::setWebUISink($sinkFile);

        if (function_exists('pcntl_fork')) {
            $pid = pcntl_fork();
            if ($pid === -1) {
                return;
            }
            if ($pid === 0) {
                // 子进程：运行 HTTP 服务器
                self::serve($port, $sinkFile, $host);
                exit(0);
            }
            // 父进程：退出时清理
            register_shutdown_function(static function () use ($pid, $sinkFile): void {
                if (function_exists('posix_kill')) {
                    @posix_kill($pid, 15); // SIGTERM
                }
                @unlink($sinkFile);
            });
        } else {
            self::spawnProcess($port, $sinkFile, $host);
        }
    }

    /**
     * 在无 pcntl 环境下，将服务器逻辑写入临时脚本并用 proc_open 后台启动。
     */
    private static function spawnProcess(int $port, string $sinkFile, string $host = '127.0.0.1'): void
    {
        $script = sys_get_temp_dir() . '/tempmail_webui_srv_' . getmypid() . '.php';
        file_put_contents($script, self::buildServeScript($port, $sinkFile, $host));
        $descriptors = [['pipe', 'r'], ['file', '/dev/null', 'w'], ['file', '/dev/null', 'w']];
        $proc = @proc_open(PHP_BINARY . ' ' . escapeshellarg($script), $descriptors, $pipes);
        if (is_resource($proc)) {
            register_shutdown_function(static function () use ($script): void {
                @unlink($script);
            });
        }
    }

    /**
     * 生成自包含的服务器脚本（不依赖 autoloader，用于无 pcntl 环境）。
     */
    private static function buildServeScript(int $port, string $sinkFile, string $host = '127.0.0.1'): string
    {
        // 用 base64 嵌入 HTML，避免转义问题
        $htmlB64 = base64_encode(self::HTML);
        $sinkEsc = addslashes($sinkFile);
        $hostEsc = addslashes($host);
        return <<<PHP
<?php
\$html = base64_decode('{$htmlB64}');
\$sink = '{$sinkEsc}';
\$host = '{$hostEsc}';
\$addr = {$port} > 0 ? "tcp://{\$host}:{$port}" : "tcp://{\$host}:0";
\$server = stream_socket_server(\$addr, \$errno, \$errstr);
if (!\$server) exit(1);
\$name = stream_socket_get_name(\$server, false);
\$actualPort = (int)substr(\$name, strrpos(\$name, ':') + 1);
fwrite(STDERR, "[WebUI] PHP WebUI 已启动，访问 http://{\$host}:{\$actualPort}\n");
stream_set_blocking(\$server, false);
while (true) {
    \$r = [\$server]; \$w = null; \$e = null;
    if (@stream_select(\$r, \$w, \$e, 1) < 1) continue;
    \$client = @stream_socket_accept(\$server, 0.0);
    if (!\$client) continue;
    stream_set_blocking(\$client, true);
    \$raw = '';
    while (true) {
        \$chunk = fread(\$client, 4096);
        if (\$chunk === false || \$chunk === '') break;
        \$raw .= \$chunk;
        if (strpos(\$raw, "\r\n\r\n") !== false) break;
    }
    \$parts = explode(' ', strtok(\$raw, "\r\n") ?: '');
    \$path = \$parts[1] ?? '/';
    if ((\$q = strpos(\$path, '?')) !== false) \$path = substr(\$path, 0, \$q);
    if (\$path === '/') {
        fwrite(\$client, "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " . strlen(\$html) . "\r\nConnection: close\r\n\r\n" . \$html);
        fclose(\$client);
    } elseif (\$path === '/api/info') {
        \$b = json_encode(['language'=>'PHP','version'=>'1.0.0','host'=>\$host,'port'=>\$actualPort,'proxy'=>'','timeout'=>15,'config'=>['telemetry'=>true,'insecure'=>false]]);
        fwrite(\$client, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " . strlen(\$b) . "\r\nConnection: close\r\n\r\n" . \$b);
        fclose(\$client);
    } elseif (\$path === '/api/channels') {
        fwrite(\$client, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 2\r\nConnection: close\r\n\r\n[]");
        fclose(\$client);
    } elseif (\$path === '/api/logs/stream') {
        fwrite(\$client, "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\nConnection: keep-alive\r\n\r\n");
        \$offset = 0;
        while (true) {
            if (file_exists(\$sink)) {
                \$content = file_get_contents(\$sink, false, null, \$offset);
                if (\$content !== false && \$content !== '') {
                    \$offset += strlen(\$content);
                    foreach (explode("\n", trim(\$content)) as \$ln) {
                        if (\$ln === '') continue;
                        if (@fwrite(\$client, "data: \$ln\n\n") === false) break 2;
                    }
                }
            }
            usleep(200000);
        }
        fclose(\$client);
    } else {
        fwrite(\$client, "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        fclose(\$client);
    }
}
PHP;
    }

    /**
     * 在当前进程（子进程）中运行 HTTP 服务器事件循环。
     * 使用 stream_select 单线程处理连接；SSE 连接独占循环直到客户端断开。
     */
    public static function serve(int $port, string $sinkFile, string $host = '127.0.0.1'): void
    {
        $addr = $port > 0 ? "tcp://{$host}:{$port}" : "tcp://{$host}:0";
        $server = stream_socket_server($addr, $errno, $errstr);
        if (!$server) {
            return;
        }
        stream_set_blocking($server, false);

        $name = stream_socket_get_name($server, false);
        $actualPort = (int)substr($name, strrpos($name, ':') + 1);
        fwrite(STDERR, "[WebUI] PHP WebUI 已启动，访问 http://{$host}:{$actualPort}\n");

        $html = self::HTML;
        $config = ConfigStore::get();

        while (true) {
            $read = [$server];
            $write = null;
            $except = null;
            if (@stream_select($read, $write, $except, 1) < 1) {
                continue;
            }
            $client = @stream_socket_accept($server, 0.0);
            if (!$client) {
                continue;
            }

            stream_set_blocking($client, true);
            $raw = '';
            while (true) {
                $chunk = fread($client, 4096);
                if ($chunk === false || $chunk === '') {
                    break;
                }
                $raw .= $chunk;
                if (strpos($raw, "\r\n\r\n") !== false) {
                    break;
                }
            }

            $parts = explode(' ', strtok($raw, "\r\n") ?: '');
            $path = $parts[1] ?? '/';
            if (($qpos = strpos($path, '?')) !== false) {
                $path = substr($path, 0, $qpos);
            }

            switch ($path) {
                case '/':
                    self::respond($client, 200, 'text/html; charset=utf-8', $html);
                    fclose($client);
                    break;

                case '/api/info':
                    $body = (string)json_encode([
                        'language' => 'PHP',
                        'version'  => '1.0.0',
                        'host'     => $host,
                        'port'     => $actualPort,
                        'proxy'    => $config->proxy ?? '',
                        'timeout'  => $config->timeout,
                        'config'   => [
                            'telemetry' => $config->telemetryEnabled !== false,
                            'insecure'  => $config->insecure,
                        ],
                    ], JSON_UNESCAPED_UNICODE);
                    self::respond($client, 200, 'application/json', $body);
                    fclose($client);
                    break;

                case '/api/channels':
                    $list = [];
                    foreach (TempMail::listChannels() as $ch) {
                        $list[] = ['channel' => $ch->channel, 'name' => $ch->name, 'website' => $ch->website];
                    }
                    self::respond($client, 200, 'application/json', (string)json_encode($list, JSON_UNESCAPED_UNICODE));
                    fclose($client);
                    break;

                case '/api/logs/stream':
                    fwrite($client,
                        "HTTP/1.1 200 OK\r\n" .
                        "Content-Type: text/event-stream\r\n" .
                        "Cache-Control: no-cache\r\n" .
                        "Connection: keep-alive\r\n\r\n"
                    );
                    $offset = 0;
                    while (true) {
                        if (file_exists($sinkFile)) {
                            $content = file_get_contents($sinkFile, false, null, $offset);
                            if ($content !== false && $content !== '') {
                                $offset += strlen($content);
                                foreach (explode("\n", trim($content)) as $ln) {
                                    if ($ln === '') {
                                        continue;
                                    }
                                    if (@fwrite($client, "data: {$ln}\n\n") === false) {
                                        break 2;
                                    }
                                }
                            }
                        }
                        usleep(200000);
                    }
                    fclose($client);
                    break;

                default:
                    fwrite($client, "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                    fclose($client);
                    break;
            }
        }
    }

    /**
     * 写入标准 HTTP 响应（非 SSE）。
     *
     * @param resource $client
     */
    private static function respond($client, int $status, string $contentType, string $body): void
    {
        $statusText = $status === 200 ? 'OK' : 'Not Found';
        fwrite($client,
            "HTTP/1.1 {$status} {$statusText}\r\n" .
            "Content-Type: {$contentType}\r\n" .
            "Content-Length: " . strlen($body) . "\r\n" .
            "Connection: close\r\n\r\n" .
            $body
        );
    }
}
