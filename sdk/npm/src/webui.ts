/**
 * WebUI 嵌入式 HTTP 服务器
 *
 * 通过环境变量激活（默认不启动）：
 *   TEMPMAIL_WEBUI=true        - 启用 WebUI
 *   TEMPMAIL_WEBUI_PORT=<端口> - 指定端口，未设置则由 OS 随机分配
 *
 * 仅使用 Node.js 内置 http 模块，无外部依赖。
 * 提供渠道列表、SDK 配置概览与实时日志（SSE）三类信息。
 */

import * as http from "http";
import { getConfig } from "./config";
import { getSdkVersion } from "./version";
import { getLogger, setLogger, LogHandler } from "./logger";
/* 触发渠道注册表填充（副作用导入） */
import "./registry_channels";
import { channelRegistry } from "./registry";

/** 单条日志推送给前端时的结构 */
interface LogEntry {
  /** 时间戳，格式 HH:MM:SS */
  time: string;
  /** 日志级别：debug / info / warn / error */
  level: string;
  /** 渠道标识（SDK 日志无结构化渠道字段时为空） */
  channel: string;
  /** 日志正文 */
  msg: string;
}

/** 当前运行的 HTTP 服务器实例，未启动时为 null */
let server: http.Server | null = null;
/** 活跃的 SSE 连接集合，断连时移除 */
const sseClients = new Set<http.ServerResponse>();
/** WebUI 启动前的原始日志处理器，停止时用于还原 */
let originalLogger: LogHandler | null = null;

/* HTML 模板占位符，构建时内联真实内容 */
const HTML_TEMPLATE = "<!DOCTYPE html>\n<html lang=\"zh-CN\">\n<head>\n<meta charset=\"UTF-8\">\n<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n<title>TempMail SDK - WebUI</title>\n<script src=\"https://cdn.tailwindcss.com\"></script>\n<script src=\"https://unpkg.com/vue@3/dist/vue.global.prod.js\"></script>\n<style>\n[v-cloak]{display:none}\n.log-debug{color:#6b7280}.log-info{color:#3b82f6}.log-warn{color:#f59e0b}.log-error{color:#ef4444}\n.fade-enter-active{transition:opacity .3s}.fade-enter-from{opacity:0}\n#log-container{max-height:60vh;overflow-y:auto;scroll-behavior:smooth}\n</style>\n</head>\n<body class=\"bg-gray-900 text-gray-100 min-h-screen\">\n<div id=\"app\" v-cloak>\n  <header class=\"bg-gray-800 border-b border-gray-700 px-6 py-4 flex items-center justify-between\">\n    <div class=\"flex items-center gap-3\">\n      <h1 class=\"text-xl font-bold\">TempMail SDK</h1>\n      <span class=\"text-sm text-gray-400\">{{ sdkInfo.language }} · v{{ sdkInfo.version }}</span>\n    </div>\n    <div class=\"flex items-center gap-4 text-sm\">\n      <span class=\"px-2 py-1 rounded\" :class=\"connected ? 'bg-green-900 text-green-300' : 'bg-red-900 text-red-300'\">\n        {{ connected ? '已连接' : '已断开' }}\n      </span>\n      <span class=\"text-gray-400\">端口 {{ sdkInfo.port }}</span>\n    </div>\n  </header>\n\n  <nav class=\"bg-gray-800 border-b border-gray-700 px-6\">\n    <div class=\"flex gap-1\">\n      <button v-for=\"tab in tabs\" :key=\"tab.id\" @click=\"activeTab=tab.id\"\n        class=\"px-4 py-2 text-sm rounded-t transition-colors\"\n        :class=\"activeTab===tab.id ? 'bg-gray-900 text-white' : 'text-gray-400 hover:text-gray-200'\">\n        {{ tab.label }}\n      </button>\n    </div>\n  </nav>\n\n  <main class=\"p-6\">\n    <!-- 概览 -->\n    <section v-show=\"activeTab==='overview'\">\n      <div class=\"grid grid-cols-1 md:grid-cols-3 gap-4 mb-6\">\n        <div class=\"bg-gray-800 rounded-lg p-4\">\n          <div class=\"text-gray-400 text-sm\">渠道总数</div>\n          <div class=\"text-2xl font-bold\">{{ channels.length }}</div>\n        </div>\n        <div class=\"bg-gray-800 rounded-lg p-4\">\n          <div class=\"text-gray-400 text-sm\">代理</div>\n          <div class=\"text-lg font-mono\">{{ sdkInfo.proxy || '未设置' }}</div>\n        </div>\n        <div class=\"bg-gray-800 rounded-lg p-4\">\n          <div class=\"text-gray-400 text-sm\">超时</div>\n          <div class=\"text-lg\">{{ sdkInfo.timeout }}s</div>\n        </div>\n      </div>\n      <div class=\"bg-gray-800 rounded-lg p-4\">\n        <h3 class=\"text-sm text-gray-400 mb-2\">配置</h3>\n        <pre class=\"text-xs font-mono text-gray-300 whitespace-pre-wrap\">{{ JSON.stringify(sdkInfo.config, null, 2) }}</pre>\n      </div>\n    </section>\n\n    <!-- 渠道 -->\n    <section v-show=\"activeTab==='channels'\">\n      <div class=\"mb-4\">\n        <input v-model=\"channelSearch\" type=\"text\" placeholder=\"搜索渠道...\"\n          class=\"bg-gray-800 border border-gray-600 rounded px-3 py-2 text-sm w-64 focus:outline-none focus:border-blue-500\">\n      </div>\n      <div class=\"bg-gray-800 rounded-lg overflow-hidden\">\n        <table class=\"w-full text-sm\">\n          <thead class=\"bg-gray-750\">\n            <tr class=\"border-b border-gray-700\">\n              <th class=\"px-4 py-2 text-left text-gray-400\">#</th>\n              <th class=\"px-4 py-2 text-left text-gray-400\">渠道标识</th>\n              <th class=\"px-4 py-2 text-left text-gray-400\">名称</th>\n              <th class=\"px-4 py-2 text-left text-gray-400\">网站</th>\n            </tr>\n          </thead>\n          <tbody>\n            <tr v-for=\"(ch, i) in filteredChannels\" :key=\"ch.channel\"\n              class=\"border-b border-gray-700/50 hover:bg-gray-700/30\">\n              <td class=\"px-4 py-2 text-gray-500\">{{ i + 1 }}</td>\n              <td class=\"px-4 py-2 font-mono\">{{ ch.channel }}</td>\n              <td class=\"px-4 py-2\">{{ ch.name }}</td>\n              <td class=\"px-4 py-2\"><a :href=\"'https://'+ch.website\" target=\"_blank\" class=\"text-blue-400 hover:underline\">{{ ch.website }}</a></td>\n            </tr>\n          </tbody>\n        </table>\n      </div>\n    </section>\n\n    <!-- 日志 -->\n    <section v-show=\"activeTab==='logs'\">\n      <div class=\"flex items-center gap-3 mb-4\">\n        <select v-model=\"logLevel\" class=\"bg-gray-800 border border-gray-600 rounded px-2 py-1 text-sm\">\n          <option value=\"all\">全部</option>\n          <option value=\"debug\">Debug</option>\n          <option value=\"info\">Info</option>\n          <option value=\"warn\">Warn</option>\n          <option value=\"error\">Error</option>\n        </select>\n        <button @click=\"logs=[]\" class=\"text-sm text-gray-400 hover:text-white\">清空</button>\n        <label class=\"text-sm text-gray-400 flex items-center gap-1\">\n          <input type=\"checkbox\" v-model=\"autoScroll\"> 自动滚动\n        </label>\n        <span class=\"text-xs text-gray-500 ml-auto\">{{ logs.length }} 条</span>\n      </div>\n      <div id=\"log-container\" ref=\"logContainer\" class=\"bg-gray-950 rounded-lg p-4 font-mono text-xs\">\n        <div v-for=\"(log, i) in filteredLogs\" :key=\"i\" class=\"py-0.5\" :class=\"'log-'+log.level\">\n          <span class=\"text-gray-600\">{{ log.time }}</span>\n          <span class=\"px-1\">[{{ log.level.toUpperCase() }}]</span>\n          <span v-if=\"log.channel\" class=\"text-purple-400\">[{{ log.channel }}]</span>\n          {{ log.msg }}\n        </div>\n        <div v-if=\"filteredLogs.length===0\" class=\"text-gray-600\">暂无日志</div>\n      </div>\n    </section>\n  </main>\n</div>\n<script>\nconst {createApp, ref, computed, onMounted, onUnmounted, nextTick, watch} = Vue\ncreateApp({\n  setup() {\n    const tabs = [{id:'overview',label:'概览'},{id:'channels',label:'渠道'},{id:'logs',label:'日志'}]\n    const activeTab = ref('overview')\n    const connected = ref(false)\n    const sdkInfo = ref({language:'',version:'',port:'',proxy:'',timeout:30,config:{}})\n    const channels = ref([])\n    const logs = ref([])\n    const channelSearch = ref('')\n    const logLevel = ref('all')\n    const autoScroll = ref(true)\n    const logContainer = ref(null)\n    let evtSource = null\n\n    const filteredChannels = computed(() => {\n      if (!channelSearch.value) return channels.value\n      const q = channelSearch.value.toLowerCase()\n      return channels.value.filter(c =>\n        c.channel.includes(q) || c.name.toLowerCase().includes(q) || c.website.includes(q))\n    })\n    const filteredLogs = computed(() => {\n      if (logLevel.value === 'all') return logs.value\n      return logs.value.filter(l => l.level === logLevel.value)\n    })\n\n    async function fetchInfo() {\n      try {\n        const r = await fetch('/api/info')\n        if (r.ok) sdkInfo.value = await r.json()\n      } catch(e) {}\n    }\n    async function fetchChannels() {\n      try {\n        const r = await fetch('/api/channels')\n        if (r.ok) channels.value = await r.json()\n      } catch(e) {}\n    }\n    function connectSSE() {\n      evtSource = new EventSource('/api/logs/stream')\n      evtSource.onopen = () => { connected.value = true }\n      evtSource.onmessage = (e) => {\n        try {\n          const entry = JSON.parse(e.data)\n          logs.value.push(entry)\n          if (logs.value.length > 5000) logs.value.splice(0, 500)\n          if (autoScroll.value) {\n            nextTick(() => {\n              const el = logContainer.value\n              if (el) el.scrollTop = el.scrollHeight\n            })\n          }\n        } catch(err) {}\n      }\n      evtSource.onerror = () => {\n        connected.value = false\n        setTimeout(connectSSE, 3000)\n      }\n    }\n\n    onMounted(() => {\n      fetchInfo()\n      fetchChannels()\n      connectSSE()\n    })\n    onUnmounted(() => { if (evtSource) evtSource.close() })\n\n    return {tabs,activeTab,connected,sdkInfo,channels,logs,channelSearch,logLevel,\n            autoScroll,logContainer,filteredChannels,filteredLogs}\n  }\n}).mount('#app')\n</script>\n\n</body>\n</html>\n";

/**
 * 生成两位数字时间片段
 */
function pad2(n: number): string {
  return n < 10 ? `0${n}` : String(n);
}

/**
 * 构造当前时刻的 HH:MM:SS 字符串
 */
function nowTime(): string {
  const d = new Date();
  return `${pad2(d.getHours())}:${pad2(d.getMinutes())}:${pad2(d.getSeconds())}`;
}

/**
 * 将日志正文与附加参数拼接为单行文本
 */
function formatMessage(msg: string, args: any[]): string {
  if (!args || args.length === 0) return msg;
  const extra = args
    .map((a) => {
      if (typeof a === "string") return a;
      try {
        return JSON.stringify(a);
      } catch {
        return String(a);
      }
    })
    .join(" ");
  return extra ? `${msg} ${extra}` : msg;
}

/**
 * 向所有活跃 SSE 连接推送一条日志
 */
function broadcastLog(entry: LogEntry): void {
  const payload = `data: ${JSON.stringify(entry)}\n\n`;
  for (const client of sseClients) {
    try {
      client.write(payload);
    } catch {
      /* 写入失败的连接在其 close 事件中已移除，这里忽略 */
    }
  }
}

/**
 * 构造包裹原处理器的日志处理器
 * 在转发给原处理器的同时，把日志推送到所有 SSE 连接
 */
function makeWrappedLogger(base: LogHandler): LogHandler {
  const emit = (level: string, msg: string, args: any[]): void => {
    broadcastLog({
      time: nowTime(),
      level,
      channel: "",
      msg: formatMessage(msg, args),
    });
  };
  return {
    error(msg: string, ...args: any[]): void {
      base.error(msg, ...args);
      emit("error", msg, args);
    },
    warn(msg: string, ...args: any[]): void {
      base.warn(msg, ...args);
      emit("warn", msg, args);
    },
    info(msg: string, ...args: any[]): void {
      base.info(msg, ...args);
      emit("info", msg, args);
    },
    debug(msg: string, ...args: any[]): void {
      base.debug(msg, ...args);
      emit("debug", msg, args);
    },
  };
}

/**
 * 以 JSON 形式响应
 */
function sendJson(res: http.ServerResponse, data: unknown): void {
  const body = JSON.stringify(data);
  res.writeHead(200, {
    "Content-Type": "application/json; charset=utf-8",
  });
  res.end(body);
}

/**
 * 处理 GET /api/info：返回运行时与配置概览
 */
function handleInfo(res: http.ServerResponse): void {
  const cfg = getConfig();
  const addr = server?.address();
  const port =
    addr && typeof addr === "object" ? addr.port : 0;
  const host =
    addr && typeof addr === "object" ? addr.address : "127.0.0.1";
  /* SDK 内部超时以毫秒记录，对外以秒展示，未配置时取默认 15s */
  const timeoutSec = Math.round((cfg.timeout ?? 15000) / 1000);
  sendJson(res, {
    language: "Node.js",
    version: getSdkVersion(),
    host,
    port,
    proxy: cfg.proxy || "",
    timeout: timeoutSec,
    config: {
      telemetry: cfg.telemetryEnabled !== false,
      insecure: cfg.insecure === true,
    },
  });
}

/**
 * 处理 GET /api/channels：返回全部渠道列表
 */
function handleChannels(res: http.ServerResponse): void {
  const list = channelRegistry.map((spec) => ({
    channel: spec.channel,
    name: spec.name,
    website: spec.website,
  }));
  sendJson(res, list);
}

/**
 * 处理 GET /api/logs/stream：建立 SSE 长连接
 */
function handleLogStream(
  req: http.IncomingMessage,
  res: http.ServerResponse,
): void {
  res.writeHead(200, {
    "Content-Type": "text/event-stream; charset=utf-8",
    "Cache-Control": "no-cache",
    Connection: "keep-alive",
  });
  /* 首帧注释保持连接活性 */
  res.write(": connected\n\n");
  sseClients.add(res);
  const cleanup = (): void => {
    sseClients.delete(res);
  };
  req.on("close", cleanup);
  res.on("close", cleanup);
}

/**
 * 处理 GET /：返回内联的前端 HTML
 */
function handleIndex(res: http.ServerResponse): void {
  res.writeHead(200, { "Content-Type": "text/html; charset=utf-8" });
  res.end(HTML_TEMPLATE);
}

/**
 * 统一请求分发
 */
function handleRequest(
  req: http.IncomingMessage,
  res: http.ServerResponse,
): void {
  const url = (req.url || "/").split("?")[0];
  if (req.method !== "GET") {
    res.writeHead(405, { "Content-Type": "text/plain; charset=utf-8" });
    res.end("Method Not Allowed");
    return;
  }
  switch (url) {
    case "/":
      handleIndex(res);
      return;
    case "/api/info":
      handleInfo(res);
      return;
    case "/api/channels":
      handleChannels(res);
      return;
    case "/api/logs/stream":
      handleLogStream(req, res);
      return;
    default:
      res.writeHead(404, { "Content-Type": "text/plain; charset=utf-8" });
      res.end("Not Found");
  }
}

/**
 * 启动 WebUI HTTP 服务器
 *
 * 端口优先取环境变量 TEMPMAIL_WEBUI_PORT，未设置或非法时传 0 让 OS 随机分配。
 * 启动后包裹当前日志处理器，将日志实时推送到 SSE 连接。
 *
 * @returns 实际监听的端口号；已在运行时返回既有端口
 */
export function startWebUI(): Promise<number> {
  if (server) {
    const addr = server.address();
    const existing =
      addr && typeof addr === "object" ? addr.port : 0;
    return Promise.resolve(existing);
  }

  /* 解析端口：非法或未设置一律回退到 0（OS 随机分配） */
  let port = 0;
  const rawPort = process.env.TEMPMAIL_WEBUI_PORT?.trim();
  if (rawPort) {
    const p = parseInt(rawPort, 10);
    if (!isNaN(p) && p >= 0 && p <= 65535) {
      port = p;
    }
  }

  /* 解析 host：未设置时默认 127.0.0.1 */
  let host = "127.0.0.1";
  const rawHost = process.env.TEMPMAIL_WEBUI_HOST?.trim();
  if (rawHost) {
    host = rawHost;
  }

  return new Promise<number>((resolve, reject) => {
    const s = http.createServer(handleRequest);
    s.on("error", (err) => {
      reject(err);
    });
    s.listen(port, host, () => {
      server = s;
      /* 包裹现有日志处理器，实现日志实时推送 */
      originalLogger = getLogger();
      setLogger(makeWrappedLogger(originalLogger));
      const addr = s.address();
      const actualPort =
        addr && typeof addr === "object" ? addr.port : port;
      /* 通过原处理器输出启动提示（此时已被 wrap，同样会推送到 SSE） */
      const actualHost =
        addr && typeof addr === "object" ? addr.address : host;
      getLogger().info(
        `TempMail SDK WebUI 已启动: http://${actualHost}:${actualPort}`,
      );
      resolve(actualPort);
    });
  });
}

/**
 * 停止 WebUI HTTP 服务器
 *
 * 关闭全部 SSE 连接、还原原始日志处理器并释放端口。
 */
export function stopWebUI(): Promise<void> {
  return new Promise<void>((resolve) => {
    if (!server) {
      resolve();
      return;
    }
    /* 主动关闭所有 SSE 连接 */
    for (const client of sseClients) {
      try {
        client.end();
      } catch {
        /* 忽略关闭异常 */
      }
    }
    sseClients.clear();
    /* 还原日志处理器 */
    if (originalLogger) {
      setLogger(originalLogger);
      originalLogger = null;
    }
    const s = server;
    server = null;
    s.close(() => {
      resolve();
    });
  });
}

/**
 * 模块加载时按环境变量决定是否自动启动 WebUI
 * TEMPMAIL_WEBUI 为 true/1/yes 时启动
 */
function maybeAutoStart(): void {
  const flag = process.env.TEMPMAIL_WEBUI?.trim().toLowerCase();
  if (flag === "true" || flag === "1" || flag === "yes") {
    startWebUI().catch((err) => {
      /* 启动失败仅记录，不影响 SDK 主流程 */
      getLogger().error(`WebUI 启动失败: ${(err as Error).message}`);
    });
  }
}

maybeAutoStart();
