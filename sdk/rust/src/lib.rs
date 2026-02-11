/*!
 * 临时邮箱 SDK (Rust)
 * 支持 11 个邮箱服务提供商，所有渠道返回统一标准化格式
 */

pub mod types;
pub mod logger;
pub mod retry;
pub mod normalize;
pub mod providers;
mod client;

pub use types::*;
pub use client::*;
pub use logger::{set_log_level, LogLevelFilter};
pub use retry::with_retry;
pub use normalize::normalize_email;
