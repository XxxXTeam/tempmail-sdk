/*!
 * 通用重试工具
 * 提供请求重试、超时控制、指数退避等错误恢复机制
 */

use crate::types::RetryConfig;
use std::thread;
use std::time::Duration;

/// 判断错误是否应该重试
fn should_retry(err: &str) -> bool {
    let msg = err.to_lowercase();

    // 网络级别错误
    let keywords = [
        "connection",
        "timeout",
        "timed out",
        "dns",
        "eof",
        "broken pipe",
        "refused",
        "reset",
        "network",
        "ssl",
    ];
    for kw in &keywords {
        if msg.contains(kw) {
            return true;
        }
    }

    // HTTP 429 限流
    if msg.contains("429") || msg.contains("too many requests") || msg.contains("rate limit") {
        return true;
    }

    // HTTP 4xx/5xx 错误（含状态码的错误消息）
    if msg.contains(": 4") || msg.contains(": 5") {
        return true;
    }
    if msg.contains("status: 4") || msg.contains("status: 5") {
        return true;
    }

    false
}

/// 带重试的操作执行器
///
/// 功能:
/// - 自动重试可恢复的错误（网络错误、超时、HTTP 4xx/5xx 等）
/// - 指数退避策略避免短时间内过度请求
/// - 不可恢复的错误（SDK 内部参数校验错误等）直接返回
///
/// 与 `with_retry` 相同；失败时返回 `(错误信息, 尝试次数)`
pub(crate) fn with_retry_with_attempts<T, F>(
    f: F,
    config: Option<&RetryConfig>,
) -> Result<(T, u32), (String, u32)>
where
    F: Fn() -> Result<T, String>,
{
    let cfg = config.cloned().unwrap_or_default();
    let mut last_err = String::new();

    for attempt in 0..=cfg.max_retries {
        let attempts = attempt + 1;
        match f() {
            Ok(result) => {
                if attempt > 0 {
                    log::info!("第 {} 次尝试成功", attempts);
                }
                return Ok((result, attempts));
            }
            Err(e) => {
                last_err = e.clone();

                if attempt >= cfg.max_retries || !should_retry(&e) {
                    if attempt >= cfg.max_retries && cfg.max_retries > 0 {
                        log::error!("重试 {} 次后仍失败: {}", cfg.max_retries, e);
                    } else if !should_retry(&e) {
                        log::debug!("不可重试的错误: {}", e);
                    }
                    return Err((e, attempts));
                }

                let delay_ms =
                    std::cmp::min(cfg.initial_delay_ms * 2u64.pow(attempt), cfg.max_delay_ms);
                log::warn!(
                    "请求失败 ({})，{}ms 后第 {} 次重试...",
                    e,
                    delay_ms,
                    attempt + 2
                );
                thread::sleep(Duration::from_millis(delay_ms));
            }
        }
    }

    Err((last_err, cfg.max_retries + 1))
}

pub fn with_retry<T, F>(f: F, config: Option<&RetryConfig>) -> Result<T, String>
where
    F: Fn() -> Result<T, String>,
{
    match with_retry_with_attempts(f, config) {
        Ok((v, _)) => Ok(v),
        Err((e, _)) => Err(e),
    }
}
