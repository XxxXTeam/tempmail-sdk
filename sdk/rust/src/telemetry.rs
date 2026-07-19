use crate::config::get_config;
use crate::version::sdk_version;
use regex::Regex;
use serde::Serialize;
use std::sync::{Mutex, OnceLock};
use std::thread;
use std::time::{Duration, SystemTime, UNIX_EPOCH};

const DEFAULT_TELEMETRY_URL: &str = "https://sdk-1.openel.top/v1/event";
const MAX_BATCH: usize = 32;
const FLUSH_SECS: u64 = 2;

fn email_redact_re() -> &'static Regex {
    static RE: OnceLock<Regex> = OnceLock::new();
    RE.get_or_init(|| Regex::new(r"[^\s@]{1,64}@[^\s@]{1,255}").expect("email redact regex"))
}

fn sanitize_error(msg: &str) -> String {
    let s = email_redact_re().replace_all(msg, "[redacted]").to_string();
    s.chars().take(400).collect()
}

fn now_ms() -> i64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_millis() as i64)
        .unwrap_or(0)
}

#[derive(Serialize, Clone)]
struct TelemetryEvent {
    operation: String,
    channel: String,
    success: bool,
    attempt_count: u32,
    #[serde(skip_serializing_if = "Option::is_none")]
    channels_tried: Option<u32>,
    #[serde(skip_serializing_if = "str::is_empty")]
    error: String,
    ts_ms: i64,
}

#[derive(Serialize)]
struct TelemetryBatch<'a> {
    schema_version: i32,
    sdk_language: &'static str,
    sdk_version: &'a str,
    os: &'static str,
    arch: &'static str,
    events: Vec<TelemetryEvent>,
}

static QUEUE: Mutex<Vec<TelemetryEvent>> = Mutex::new(Vec::new());
static FLUSH_LOCK: Mutex<()> = Mutex::new(());

fn ensure_flush_thread() {
    static START: OnceLock<()> = OnceLock::new();
    START.get_or_init(|| {
        thread::spawn(|| loop {
            thread::sleep(Duration::from_secs(FLUSH_SECS));
            flush_telemetry_queue();
        });
    });
}

fn flush_telemetry_queue() {
    let cfg = get_config();
    if !cfg.telemetry_is_enabled() {
        QUEUE.lock().unwrap().clear();
        return;
    }

    let url = cfg
        .telemetry_url
        .as_deref()
        .filter(|u| !u.trim().is_empty())
        .unwrap_or(DEFAULT_TELEMETRY_URL)
        .to_string();

    let _guard = FLUSH_LOCK.lock().unwrap();

    let events = {
        let mut q = QUEUE.lock().unwrap();
        if q.is_empty() {
            return;
        }
        std::mem::take(&mut *q)
    };

    let ver = sdk_version();
    let batch = TelemetryBatch {
        schema_version: 2,
        sdk_language: "rust",
        sdk_version: ver,
        os: std::env::consts::OS,
        arch: std::env::consts::ARCH,
        events,
    };

    let body = match serde_json::to_vec(&batch) {
        Ok(b) => b,
        Err(_) => return,
    };

    let ua = format!("tempmail-sdk-rust/{}", ver);
    thread::spawn(move || {
        crate::block_on(async move {
            let client = match wreq::Client::builder()
                .timeout(Duration::from_secs(8))
                .build()
            {
                Ok(c) => c,
                Err(_) => return,
            };
            let _ = client
                .post(&url)
                .header("Content-Type", "application/json")
                .header("User-Agent", &ua)
                .body(body)
                .send()
                .await;
        });
    });
}

/// 入队，与进程内其它事件合并后统一 POST
pub fn report_telemetry(
    operation: &str,
    channel: &str,
    success: bool,
    attempt_count: u32,
    channels_tried: u32,
    error: &str,
) {
    let cfg = get_config();
    if !cfg.telemetry_is_enabled() {
        return;
    }

    ensure_flush_thread();

    let ev = TelemetryEvent {
        operation: operation.to_string(),
        channel: channel.to_string(),
        success,
        attempt_count,
        channels_tried: if channels_tried > 0 {
            Some(channels_tried)
        } else {
            None
        },
        error: sanitize_error(error),
        ts_ms: now_ms(),
    };

    let n = {
        let mut q = QUEUE.lock().unwrap();
        q.push(ev);
        q.len()
    };

    if n >= MAX_BATCH {
        flush_telemetry_queue();
    }
}
