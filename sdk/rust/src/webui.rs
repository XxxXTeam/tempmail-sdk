/*!
 * WebUI 嵌入式 HTTP 服务器
 *
 * 仅依赖标准库实现的极简 HTTP/1.1 服务器，用于可视化查看 SDK 运行状态、
 * 渠道列表与实时日志。默认不启动，通过环境变量激活：
 *   TEMPMAIL_WEBUI       - 设为 "true"/"1"/"yes" 时激活
 *   TEMPMAIL_WEBUI_HOST  - 指定监听 host；未设置则默认 127.0.0.1
 *   TEMPMAIL_WEBUI_PORT  - 指定监听端口；未设置则由系统随机分配（bind :0）
 *
 * 路由：
 *   GET /                 - 返回前端 HTML 页面
 *   GET /api/info         - 返回 SDK 运行信息 JSON
 *   GET /api/channels     - 返回渠道列表 JSON 数组
 *   GET /api/logs/stream  - SSE 实时日志流
 *
 * 日志挂钩：安装自定义 log::Log 实现，将日志同时输出到 stderr 与 SSE 客户端。
 */

use crate::{config, registry, sdk_version};
use std::io::{BufRead, BufReader, Write};
use std::net::{TcpListener, TcpStream};
use std::sync::atomic::{AtomicBool, AtomicU16, Ordering};
use std::sync::{Mutex, OnceLock};

/* 嵌入的前端页面（Vue 脚本已内联，构建时编译进二进制） */
const INDEX_HTML: &str = include_str!("webui_template.html");

/* WebUI 实际监听端口（bind 后写入，供 /api/info 返回） */
static WEBUI_PORT: AtomicU16 = AtomicU16::new(0);

/* WebUI 实际监听 host */
static WEBUI_HOST: OnceLock<String> = OnceLock::new();

/* WebUI 是否已启动（防止重复启动） */
static WEBUI_STARTED: AtomicBool = AtomicBool::new(false);

/// SSE 客户端连接池：每个元素是一个已发送 SSE 响应头、等待推送日志的 TcpStream
fn sse_clients() -> &'static Mutex<Vec<TcpStream>> {
    static CLIENTS: OnceLock<Mutex<Vec<TcpStream>>> = OnceLock::new();
    CLIENTS.get_or_init(|| Mutex::new(Vec::new()))
}

/// 自定义日志实现：把日志写入 stderr，同时以 SSE 事件推送给所有 WebUI 客户端。
/// 使用 log crate 的全局 logger 挂钩，仅在 WebUI 启动时安装一次。
struct WebUILogger;

impl log::Log for WebUILogger {
    fn enabled(&self, _metadata: &log::Metadata) -> bool {
        true
    }

    fn log(&self, record: &log::Record) {
        /* log crate 会先按 max_level 过滤，这里再按 enabled 兜底 */
        if !self.enabled(record.metadata()) {
            return;
        }

        let level = match record.level() {
            log::Level::Error => "error",
            log::Level::Warn => "warn",
            log::Level::Info => "info",
            log::Level::Debug => "debug",
            log::Level::Trace => "debug",
        };
        let time = current_time_str();
        let msg = record.args().to_string();

        /* 输出到 stderr，保持与默认行为一致 */
        eprintln!("[{}] [{}] {}", time, level.to_uppercase(), msg);

        /* 推送到 SSE 客户端 */
        broadcast_log(&time, level, &msg);
    }

    fn flush(&self) {}
}

/// 生成 `HH:MM:SS` 形式的当前时间字符串（本地时区）
fn current_time_str() -> String {
    chrono::Local::now().format("%H:%M:%S").to_string()
}

/// 把单条日志封装为 SSE 事件，广播给所有客户端；发送失败的连接自动剔除。
fn broadcast_log(time: &str, level: &str, msg: &str) {
    let payload = format!(
        "{{\"time\":\"{}\",\"level\":\"{}\",\"msg\":\"{}\"}}",
        json_escape(time),
        json_escape(level),
        json_escape(msg)
    );
    let frame = format!("data: {}\n\n", payload);

    let mut clients = match sse_clients().lock() {
        Ok(g) => g,
        Err(_) => return,
    };
    /* 写失败视为断开，予以移除 */
    clients.retain_mut(|stream| stream.write_all(frame.as_bytes()).is_ok());
}

/// JSON 字符串转义：处理引号、反斜杠与常见控制字符。
fn json_escape(s: &str) -> String {
    let mut out = String::with_capacity(s.len() + 2);
    for c in s.chars() {
        match c {
            '"' => out.push_str("\\\""),
            '\\' => out.push_str("\\\\"),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            c if (c as u32) < 0x20 => out.push_str(&format!("\\u{:04x}", c as u32)),
            c => out.push(c),
        }
    }
    out
}

/// 构造 `/api/info` 响应 JSON。
/// 结构: {language, version, port, proxy, timeout, config:{telemetry, insecure}}
fn build_info_json() -> String {
    let cfg = config::get_config();
    let port = WEBUI_PORT.load(Ordering::SeqCst);
    let host = WEBUI_HOST.get().map(|s| s.as_str()).unwrap_or("127.0.0.1");
    let proxy = match &cfg.proxy {
        Some(p) => format!("\"{}\"", json_escape(p)),
        None => "\"\"".to_string(),
    };
    format!(
        "{{\"language\":\"Rust\",\"version\":\"{}\",\"host\":\"{}\",\"port\":{},\"proxy\":{},\"timeout\":{},\"config\":{{\"telemetry\":{},\"insecure\":{}}}}}",
        json_escape(sdk_version()),
        json_escape(host),
        port,
        proxy,
        cfg.timeout_secs,
        cfg.telemetry_is_enabled(),
        cfg.insecure
    )
}

/// 构造 `/api/channels` 响应 JSON 数组。
/// 每个元素: {channel, name, website}
fn build_channels_json() -> String {
    let list = registry::list_channels();
    let mut out = String::from("[");
    for (i, info) in list.iter().enumerate() {
        if i > 0 {
            out.push(',');
        }
        out.push_str(&format!(
            "{{\"channel\":\"{}\",\"name\":\"{}\",\"website\":\"{}\"}}",
            json_escape(&info.channel.to_string()),
            json_escape(info.name),
            json_escape(info.website)
        ));
    }
    out.push(']');
    out
}

/// 写出一个带 Content-Type 的完整 HTTP 响应（自动附加 CORS 头，便于本地调试）。
fn write_response(stream: &mut TcpStream, status: &str, content_type: &str, body: &[u8]) {
    let header = format!(
        "HTTP/1.1 {}\r\nContent-Type: {}\r\nContent-Length: {}\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n",
        status,
        content_type,
        body.len()
    );
    let _ = stream.write_all(header.as_bytes());
    let _ = stream.write_all(body);
    let _ = stream.flush();
}

/// 处理单个 TCP 连接：解析请求行，按路径分发。
fn handle_connection(mut stream: TcpStream) {
    let mut reader = BufReader::new(match stream.try_clone() {
        Ok(s) => s,
        Err(_) => return,
    });

    /* 读取请求行，如 "GET /api/info HTTP/1.1" */
    let mut request_line = String::new();
    if reader.read_line(&mut request_line).is_err() || request_line.is_empty() {
        return;
    }

    /* 读掉剩余请求头直到空行，避免半包 */
    loop {
        let mut line = String::new();
        match reader.read_line(&mut line) {
            Ok(0) => break,
            Ok(_) => {
                if line == "\r\n" || line == "\n" {
                    break;
                }
            }
            Err(_) => break,
        }
    }

    let path = request_line.split_whitespace().nth(1).unwrap_or("/");

    match path {
        "/" | "/index.html" => {
            write_response(
                &mut stream,
                "200 OK",
                "text/html; charset=utf-8",
                INDEX_HTML.as_bytes(),
            );
        }
        "/api/info" => {
            write_response(
                &mut stream,
                "200 OK",
                "application/json; charset=utf-8",
                build_info_json().as_bytes(),
            );
        }
        "/api/channels" => {
            write_response(
                &mut stream,
                "200 OK",
                "application/json; charset=utf-8",
                build_channels_json().as_bytes(),
            );
        }
        "/api/logs/stream" => {
            handle_sse(stream);
        }
        _ => {
            write_response(
                &mut stream,
                "404 Not Found",
                "text/plain; charset=utf-8",
                b"not found",
            );
        }
    }
}

/// 处理 SSE 订阅：写出 SSE 响应头后，把连接放入客户端池由日志钩子持续推送。
fn handle_sse(mut stream: TcpStream) {
    let header = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\nConnection: keep-alive\r\nAccess-Control-Allow-Origin: *\r\n\r\n";
    if stream.write_all(header.as_bytes()).is_err() {
        return;
    }
    let _ = stream.flush();
    /* 发一条注释帧，确保浏览器立即建立连接 */
    let _ = stream.write_all(b": connected\n\n");
    let _ = stream.flush();

    if let Ok(mut clients) = sse_clients().lock() {
        clients.push(stream);
    }
}

/// 启动 WebUI 服务器（幂等，多次调用只生效一次）。
///
/// - `host` 为 `Some(h)` 时监听指定 host；`None` 时默认 `127.0.0.1`。
/// - `port` 为 `Some(p)` 时监听指定端口；`None` 时由系统随机分配（bind `:0`）。
/// - 服务器在独立线程中运行，不阻塞调用方。
/// - 返回实际监听端口。
///
/// 同时安装自定义日志钩子，将 SDK 日志实时推送到 WebUI；若全局 logger 已被占用则跳过安装。
pub fn start_webui(host: Option<&str>, port: Option<u16>) -> std::io::Result<u16> {
    /* 幂等：已启动则直接返回当前端口 */
    if WEBUI_STARTED.swap(true, Ordering::SeqCst) {
        return Ok(WEBUI_PORT.load(Ordering::SeqCst));
    }

    let host = host.map(|h| h.trim()).filter(|h| !h.is_empty()).unwrap_or("127.0.0.1");
    let addr = format!("{}:{}", host, port.unwrap_or(0));
    let listener = TcpListener::bind(&addr)?;
    let actual_port = listener.local_addr()?.port();
    WEBUI_PORT.store(actual_port, Ordering::SeqCst);
    let _ = WEBUI_HOST.set(host.to_string());

    /* 安装日志钩子：set_logger 只能成功一次，失败说明用户已装其它 logger，忽略即可 */
    if log::set_logger(&WebUILogger).is_ok() {
        log::set_max_level(log::LevelFilter::Debug);
    }

    /* 独立线程 accept 循环，每个连接再开线程处理，避免慢连接阻塞 */
    std::thread::spawn(move || {
        for stream in listener.incoming() {
            match stream {
                Ok(s) => {
                    std::thread::spawn(move || handle_connection(s));
                }
                Err(e) => {
                    log::warn!("WebUI 接受连接失败: {}", e);
                }
            }
        }
    });

    log::info!("WebUI 已启动: http://{}:{}", host, actual_port);
    Ok(actual_port)
}

/// 依据环境变量决定是否启动 WebUI。
///
/// 仅当 `TEMPMAIL_WEBUI` 为 `true`/`1`/`yes`（忽略大小写）时启动；
/// host 取自 `TEMPMAIL_WEBUI_HOST`，未设置则默认 `127.0.0.1`；
/// 端口取自 `TEMPMAIL_WEBUI_PORT`，未设置或非法则随机分配。
/// 返回 `Some(port)` 表示已启动，`None` 表示未激活。
pub fn start_webui_if_enabled() -> Option<u16> {
    let enabled = std::env::var("TEMPMAIL_WEBUI")
        .ok()
        .map(|v| {
            let v = v.trim().to_ascii_lowercase();
            v == "true" || v == "1" || v == "yes"
        })
        .unwrap_or(false);
    if !enabled {
        return None;
    }

    let host = std::env::var("TEMPMAIL_WEBUI_HOST")
        .ok()
        .map(|s| s.trim().to_string())
        .filter(|s| !s.is_empty());

    let port = std::env::var("TEMPMAIL_WEBUI_PORT")
        .ok()
        .and_then(|s| s.trim().parse::<u16>().ok());

    match start_webui(host.as_deref(), port) {
        Ok(p) => Some(p),
        Err(e) => {
            log::warn!("WebUI 启动失败: {}", e);
            None
        }
    }
}

