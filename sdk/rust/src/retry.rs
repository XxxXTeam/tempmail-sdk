/*!
 * 通用重试工具
 * 提供请求重试、超时控制、指数退避等错误恢复机制
 */

use std::thread;
use std::time::Duration;
use crate::types::RetryConfig;

/// 判断错误是否应该重试
fn should_retry(err: &str) -> bool {
    let msg = err.to_lowercase();

    // 网络级别错误
    let keywords = [
        "connection", "timeout", "timed out", "dns",
        "eof", "broken pipe", "refused", "reset",
        "network", "ssl",
    ];
    for kw in &keywords {
        if msg.contains(kw) {
            return true;
        }
    }

    // HTTP 4xx/5xx 错误
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
/// - 自动重试可恢复的错误（网络错误、超时、HTTP 4xx/5xx）
/// - 指数退避策略避免短时间内过度请求
/// - 不可恢复的错误直接返回
pub fn with_retry<T, F>(f: F, config: Option<&RetryConfig>) -> Result<T, String>
where
    F: Fn() -> Result<T, String>,
{
    let cfg = config.cloned().unwrap_or_default();
    let mut last_err = String::new();

    for attempt in 0..=cfg.max_retries {
        match f() {
            Ok(result) => {
                if attempt > 0 {
                    log::info!("第 {} 次尝试成功", attempt + 1);
                }
                return Ok(result);
            }
            Err(e) => {
                last_err = e.clone();

                if attempt >= cfg.max_retries || !should_retry(&e) {
                    if attempt >= cfg.max_retries && cfg.max_retries > 0 {
                        log::error!("重试 {} 次后仍失败: {}", cfg.max_retries, e);
                    } else if !should_retry(&e) {
                        log::debug!("不可重试的错误: {}", e);
                    }
                    return Err(e);
                }

                let delay_ms = std::cmp::min(
                    cfg.initial_delay_ms * 2u64.pow(attempt),
                    cfg.max_delay_ms,
                );
                log::warn!(
                    "请求失败 ({})，{}ms 后第 {} 次重试...",
                    e, delay_ms, attempt + 2
                );
                thread::sleep(Duration::from_millis(delay_ms));
            }
        }
    }

    Err(last_err)
}
