/*!
 * SDK 日志模块
 * 基于 log crate 实现，用户可通过 set_log_level 设置级别
 * 默认静默不输出
 */

pub use log::LevelFilter as LogLevelFilter;

/// 设置日志级别，需要用户自行初始化 log 实现（如 env_logger）
/// 此函数设置全局最大日志级别
///
/// 示例:
/// ```rust
/// tempmail_sdk::set_log_level(tempmail_sdk::LogLevelFilter::Debug);
/// ```
pub fn set_log_level(level: LogLevelFilter) {
    log::set_max_level(level);
}
