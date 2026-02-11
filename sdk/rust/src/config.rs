/*!
 * SDK 全局配置
 * 包含代理、超时、SSL 等设置，作用于所有 HTTP 请求
 *
 * 支持环境变量自动读取（优先级低于代码设置）：
 *   TEMPMAIL_PROXY    - 代理 URL
 *   TEMPMAIL_TIMEOUT  - 超时秒数
 *   TEMPMAIL_INSECURE - 设为 "1" 或 "true" 跳过 SSL 验证
 */

use std::sync::{RwLock, Mutex, atomic::{AtomicU64, Ordering}};
use std::time::Duration;
use reqwest::blocking::Client;

/// SDK 全局配置
#[derive(Debug, Clone)]
pub struct SDKConfig {
    /// 代理 URL，支持 http/https/socks5，如 "http://127.0.0.1:7890"
    pub proxy: Option<String>,
    /// 全局默认超时秒数，默认 15
    pub timeout_secs: u64,
    /// 跳过 SSL 证书验证（调试用）
    pub insecure: bool,
}

impl Default for SDKConfig {
    fn default() -> Self {
        load_env_config()
    }
}

/*
 * 从环境变量读取默认配置
 * TEMPMAIL_PROXY / TEMPMAIL_TIMEOUT / TEMPMAIL_INSECURE
 */
fn load_env_config() -> SDKConfig {
    let proxy = std::env::var("TEMPMAIL_PROXY").ok().filter(|s| !s.is_empty());
    let timeout_secs = std::env::var("TEMPMAIL_TIMEOUT").ok()
        .and_then(|s| s.parse::<u64>().ok())
        .unwrap_or(15);
    let insecure = std::env::var("TEMPMAIL_INSECURE").ok()
        .map(|v| v == "1" || v.eq_ignore_ascii_case("true"))
        .unwrap_or(false);
    SDKConfig { proxy, timeout_secs, insecure }
}

static GLOBAL_CONFIG: RwLock<Option<SDKConfig>> = RwLock::new(None);
static CONFIG_VERSION: AtomicU64 = AtomicU64::new(0);

/* 缓存的 HTTP 客户端及其对应配置版本 */
static CACHED_CLIENT: Mutex<Option<(Client, u64)>> = Mutex::new(None);

/// 设置 SDK 全局配置
/// 设置后自动使已缓存的 HTTP 客户端失效，下次请求时按新配置重建
///
/// 示例:
/// ```no_run
/// tempmail_sdk::set_config(tempmail_sdk::SDKConfig {
///     proxy: Some("http://127.0.0.1:7890".into()),
///     timeout_secs: 30,
///     insecure: false,
/// });
/// ```
pub fn set_config(config: SDKConfig) {
    log::info!(
        "SDK 配置已更新: proxy={:?} timeout={}s insecure={}",
        config.proxy, config.timeout_secs, config.insecure
    );
    let mut guard = GLOBAL_CONFIG.write().unwrap();
    *guard = Some(config);
    CONFIG_VERSION.fetch_add(1, Ordering::SeqCst);
}

/// 获取当前 SDK 全局配置
pub fn get_config() -> SDKConfig {
    let guard = GLOBAL_CONFIG.read().unwrap();
    guard.clone().unwrap_or_default()
}

/// 获取带全局配置的 reqwest blocking Client
/// 内部缓存复用，仅在配置变更时重建，保证连接池复用以提升性能
pub fn http_client() -> Client {
    let ver = CONFIG_VERSION.load(Ordering::SeqCst);

    let mut guard = CACHED_CLIENT.lock().unwrap();
    if let Some((ref client, cached_ver)) = *guard {
        if cached_ver == ver {
            return client.clone();
        }
    }

    /* 缓存未命中或配置已变更，重建客户端 */
    let cfg = get_config();

    let mut builder = Client::builder()
        .user_agent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36")
        .timeout(Duration::from_secs(cfg.timeout_secs))
        .pool_max_idle_per_host(10)
        .pool_idle_timeout(Duration::from_secs(90));

    /* 代理 */
    if let Some(ref proxy_url) = cfg.proxy {
        if let Ok(proxy) = reqwest::Proxy::all(proxy_url) {
            builder = builder.proxy(proxy);
        }
    }

    /* SSL 验证 */
    if cfg.insecure {
        builder = builder.danger_accept_invalid_certs(true);
    }

    let client = builder.build().unwrap();
    *guard = Some((client.clone(), ver));
    client
}
