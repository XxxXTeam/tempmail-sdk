/*!
 * 临时邮箱 SDK (Rust)
 * 支持 176 个 channel 标识，按 67 个独立服务商归并，所有渠道返回统一标准化格式
 */

mod client;
pub mod config;
pub mod logger;
pub mod normalize;
pub mod providers;
pub mod retry;
mod telemetry;
pub mod types;
mod version;

pub use client::*;
pub use config::{block_on, get_config, get_current_ua, http_client, set_config, SDKConfig};
pub use logger::{set_log_level, LogLevelFilter};
pub use normalize::normalize_email;
pub use retry::with_retry;
pub use types::*;
pub use version::sdk_version;
