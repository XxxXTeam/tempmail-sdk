/*!
 * SDK 全局配置
 * 包含代理、超时、SSL 等设置，作用于所有 HTTP 请求
 * 底层使用 wreq 实现浏览器 TLS/HTTP2 指纹模拟，自动随机选取浏览器配置
 *
 * 支持环境变量自动读取（优先级低于代码设置）：
 *   TEMPMAIL_PROXY    - 代理 URL
 *   TEMPMAIL_TIMEOUT  - 超时秒数
 *   TEMPMAIL_INSECURE - 设为 "1" 或 "true" 跳过 SSL 验证
 *   DROPMAIL_AUTH_TOKEN / DROPMAIL_API_TOKEN - DropMail af_ 令牌（可选；未设置则自动 generate/renew）
 *   DROPMAIL_NO_AUTO_TOKEN - 禁止自动拉取/续期
 *   DROPMAIL_RENEW_LIFETIME - renew 的 lifetime，默认 1d
 */

use std::sync::{RwLock, OnceLock, atomic::{AtomicU64, Ordering}};
use std::time::Duration;
use wreq::Client;
use wreq_util::Emulation;
use rand::Rng;

/// SDK 全局配置
#[derive(Debug, Clone)]
pub struct SDKConfig {
    /// 代理 URL，支持 http/https/socks5，如 "http://127.0.0.1:7890"
    pub proxy: Option<String>,
    /// 全局默认超时秒数，默认 15
    pub timeout_secs: u64,
    /// 跳过 SSL 证书验证（调试用）
    pub insecure: bool,
    /// DropMail GraphQL 路径中的 af_ 令牌
    pub dropmail_auth_token: Option<String>,
    /// 为 true 时不自动 generate/renew
    pub dropmail_disable_auto_token: bool,
    /// /api/token/renew 的 lifetime，如 Some("1d")
    pub dropmail_renew_lifetime: Option<String>,
}

impl Default for SDKConfig {
    fn default() -> Self {
        load_env_config()
    }
}

/**
 * 浏览器指纹配置：Emulation profile 与 User-Agent 的配对
 * 确保 TLS/HTTP2 指纹与 HTTP 层 UA 一致，避免被检测
 */
struct BrowserConfig {
    emulation: Emulation,
    ua: &'static str,
}

/* 浏览器配置池：覆盖多平台多浏览器 */
static BROWSER_CONFIGS: &[BrowserConfig] = &[
    /* Chrome - Windows */
    BrowserConfig { emulation: Emulation::Chrome131, ua: "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36" },
    BrowserConfig { emulation: Emulation::Chrome133, ua: "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/133.0.0.0 Safari/537.36" },
    /* Chrome - macOS */
    BrowserConfig { emulation: Emulation::Chrome131, ua: "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36" },
    BrowserConfig { emulation: Emulation::Chrome133, ua: "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/133.0.0.0 Safari/537.36" },
    /* Chrome - Linux */
    BrowserConfig { emulation: Emulation::Chrome131, ua: "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36" },
    /* Edge - Windows（基于 Chromium，使用 Chrome 指纹） */
    BrowserConfig { emulation: Emulation::Chrome131, ua: "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36 Edg/131.0.0.0" },
    BrowserConfig { emulation: Emulation::Chrome133, ua: "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/133.0.0.0 Safari/537.36 Edg/133.0.0.0" },
    /* Firefox - Windows */
    BrowserConfig { emulation: Emulation::Firefox133, ua: "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:133.0) Gecko/20100101 Firefox/133.0" },
    BrowserConfig { emulation: Emulation::Firefox136, ua: "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:136.0) Gecko/20100101 Firefox/136.0" },
    /* Firefox - macOS */
    BrowserConfig { emulation: Emulation::Firefox133, ua: "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:133.0) Gecko/20100101 Firefox/133.0" },
    /* Firefox - Linux */
    BrowserConfig { emulation: Emulation::Firefox136, ua: "Mozilla/5.0 (X11; Linux x86_64; rv:136.0) Gecko/20100101 Firefox/136.0" },
    /* Safari - macOS */
    BrowserConfig { emulation: Emulation::Safari18_2, ua: "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.2 Safari/605.1.15" },
    /* Safari - iOS */
    BrowserConfig { emulation: Emulation::SafariIPad18, ua: "Mozilla/5.0 (iPad; CPU OS 18_2 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.2 Mobile/15E148 Safari/604.1" },
];

/* 当前选中的浏览器配置索引 */
static CURRENT_BROWSER_IDX: AtomicU64 = AtomicU64::new(0);

/// 随机选取一个浏览器配置并缓存
fn pick_random_browser() -> &'static BrowserConfig {
    let idx = rand::thread_rng().gen_range(0..BROWSER_CONFIGS.len());
    CURRENT_BROWSER_IDX.store(idx as u64, Ordering::SeqCst);
    &BROWSER_CONFIGS[idx]
}

/// 获取当前选中的浏览器配置
fn current_browser() -> &'static BrowserConfig {
    let idx = CURRENT_BROWSER_IDX.load(Ordering::SeqCst) as usize;
    &BROWSER_CONFIGS[idx.min(BROWSER_CONFIGS.len() - 1)]
}

/// 获取当前 TLS 客户端对应的 User-Agent
pub fn get_current_ua() -> &'static str {
    current_browser().ua
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
    let dropmail_auth_token = std::env::var("DROPMAIL_AUTH_TOKEN").ok()
        .filter(|s| !s.trim().is_empty())
        .or_else(|| std::env::var("DROPMAIL_API_TOKEN").ok().filter(|s| !s.trim().is_empty()));
    let dropmail_disable_auto_token = std::env::var("DROPMAIL_NO_AUTO_TOKEN").ok()
        .map(|v| {
            let v = v.trim().to_ascii_lowercase();
            v == "1" || v == "true" || v == "yes"
        })
        .unwrap_or(false);
    let dropmail_renew_lifetime = std::env::var("DROPMAIL_RENEW_LIFETIME").ok()
        .filter(|s| !s.trim().is_empty());
    SDKConfig {
        proxy,
        timeout_secs,
        insecure,
        dropmail_auth_token,
        dropmail_disable_auto_token,
        dropmail_renew_lifetime,
    }
}

static GLOBAL_CONFIG: RwLock<Option<SDKConfig>> = RwLock::new(None);
static CONFIG_VERSION: AtomicU64 = AtomicU64::new(0);

/* 缓存的 wreq 客户端及其对应配置版本（使用 RwLock 减少并发读争用） */
static CACHED_CLIENT: RwLock<Option<(Client, u64)>> = RwLock::new(None);

/* 全局 tokio Runtime（wreq 是异步的，SDK 对外暴露同步 API） */
static TOKIO_RT: OnceLock<tokio::runtime::Runtime> = OnceLock::new();

/// 获取全局 tokio Runtime
fn runtime() -> &'static tokio::runtime::Runtime {
    TOKIO_RT.get_or_init(|| {
        tokio::runtime::Builder::new_current_thread()
            .enable_all()
            .build()
            .expect("创建 tokio runtime 失败")
    })
}

/// 在同步上下文中执行异步操作（SDK 内部使用）
pub fn block_on<F: std::future::Future>(future: F) -> F::Output {
    runtime().block_on(future)
}

/// 设置 SDK 全局配置
/// 设置后自动使已缓存的 HTTP 客户端失效，下次请求时按新配置重建
pub fn set_config(config: SDKConfig) {
    log::info!(
        "SDK 配置已更新: proxy={:?} timeout={}s insecure={} dropmail_auto_disabled={}",
        config.proxy, config.timeout_secs, config.insecure, config.dropmail_disable_auto_token
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

/// 获取带全局配置和浏览器指纹的 wreq Client
/// 内部缓存复用，仅在配置变更时重建
/// 每次重建时随机选取浏览器配置，模拟真实浏览器 TLS/HTTP2 指纹
pub fn http_client() -> Client {
    let ver = CONFIG_VERSION.load(Ordering::SeqCst);

    /* 快速路径：读锁检查缓存命中（并发读不阻塞） */
    {
        let guard = CACHED_CLIENT.read().unwrap();
        if let Some((ref client, cached_ver)) = *guard {
            if cached_ver == ver {
                return client.clone();
            }
        }
    }

    /* 慢路径：写锁重建客户端（仅在配置变更或首次创建时） */
    let mut guard = CACHED_CLIENT.write().unwrap();
    /* 双重检查：可能在等写锁期间已被其他线程重建 */
    if let Some((ref client, cached_ver)) = *guard {
        if cached_ver == ver {
            return client.clone();
        }
    }

    let cfg = get_config();
    let bc = pick_random_browser();

    log::debug!("创建 wreq 客户端, emulation={:?}, ua={}", bc.emulation, bc.ua);

    let mut builder = Client::builder()
        .emulation(bc.emulation)
        .timeout(Duration::from_secs(cfg.timeout_secs))
        .cookie_store(true);

    /* 代理 */
    if let Some(ref proxy_url) = cfg.proxy {
        if let Ok(proxy) = wreq::Proxy::all(proxy_url) {
            builder = builder.proxy(proxy);
        }
    }

    /* SSL 验证 */
    if cfg.insecure {
        builder = builder.cert_verification(false);
    }

    let client = builder.build().unwrap();
    *guard = Some((client.clone(), ver));
    client
}
