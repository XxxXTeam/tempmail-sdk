/*!
 * 临时邮箱 SDK (Rust)
 * 支持 268 个 channel 标识，按 100 个独立服务商归并，所有渠道返回统一标准化格式
 */

mod backend_groups;
pub mod channel_domains;
mod client;
pub mod config;
pub mod logger;
pub mod normalize;
pub mod providers;
pub mod registry;
pub mod retry;
mod telemetry;
pub mod types;
mod version;
pub mod webui;

pub use client::*;
pub use config::{block_on, get_config, get_current_ua, http_client, set_config, SDKConfig};
pub use logger::{set_log_level, LogLevelFilter};
pub use normalize::normalize_email;
pub use registry::{all_channels, get_channel_info, list_channels};
pub use retry::with_retry;
pub use types::*;
pub use version::sdk_version;
pub use webui::{start_webui, start_webui_if_enabled};
