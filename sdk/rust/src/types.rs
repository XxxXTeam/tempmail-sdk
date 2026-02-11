/*!
 * SDK 类型定义
 */

use serde::{Deserialize, Serialize};

/// 支持的临时邮箱渠道标识
#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum Channel {
    #[serde(rename = "tempmail")]
    Tempmail,
    #[serde(rename = "linshi-email")]
    LinshiEmail,
    #[serde(rename = "tempmail-lol")]
    TempmailLol,
    #[serde(rename = "chatgpt-org-uk")]
    ChatgptOrgUk,
    #[serde(rename = "tempmail-la")]
    TempmailLa,
    #[serde(rename = "temp-mail-io")]
    TempMailIO,
    #[serde(rename = "awamail")]
    Awamail,
    #[serde(rename = "mail-tm")]
    MailTm,
    #[serde(rename = "dropmail")]
    Dropmail,
    #[serde(rename = "guerrillamail")]
    GuerrillaMail,
    #[serde(rename = "maildrop")]
    Maildrop,
}

impl std::fmt::Display for Channel {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Channel::Tempmail => write!(f, "tempmail"),
            Channel::LinshiEmail => write!(f, "linshi-email"),
            Channel::TempmailLol => write!(f, "tempmail-lol"),
            Channel::ChatgptOrgUk => write!(f, "chatgpt-org-uk"),
            Channel::TempmailLa => write!(f, "tempmail-la"),
            Channel::TempMailIO => write!(f, "temp-mail-io"),
            Channel::Awamail => write!(f, "awamail"),
            Channel::MailTm => write!(f, "mail-tm"),
            Channel::Dropmail => write!(f, "dropmail"),
            Channel::GuerrillaMail => write!(f, "guerrillamail"),
            Channel::Maildrop => write!(f, "maildrop"),
        }
    }
}

/// 创建临时邮箱后返回的邮箱信息
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EmailInfo {
    /// 创建该邮箱所使用的渠道
    pub channel: Channel,
    /// 临时邮箱地址
    pub email: String,
    /// 认证令牌，部分渠道获取邮件时需要
    pub token: Option<String>,
    /// 邮箱过期时间（毫秒时间戳）
    pub expires_at: Option<i64>,
    /// 邮箱创建时间
    pub created_at: Option<String>,
}

/// 邮件附件信息
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct EmailAttachment {
    /// 附件文件名
    pub filename: String,
    /// 附件大小（字节）
    pub size: Option<i64>,
    /// 附件 MIME 类型
    pub content_type: Option<String>,
    /// 附件下载地址
    pub url: Option<String>,
}

/// 标准化的邮件对象
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct Email {
    /// 邮件唯一标识
    pub id: String,
    /// 发件人地址
    pub from_addr: String,
    /// 收件人地址
    pub to: String,
    /// 邮件主题
    pub subject: String,
    /// 纯文本内容
    pub text: String,
    /// HTML 内容
    pub html: String,
    /// 接收日期（ISO 8601 格式）
    pub date: String,
    /// 是否已读
    pub is_read: bool,
    /// 附件列表
    pub attachments: Vec<EmailAttachment>,
}

/// 获取邮件列表的结果
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GetEmailsResult {
    /// 渠道标识
    pub channel: Channel,
    /// 邮箱地址
    pub email: String,
    /// 邮件列表
    pub emails: Vec<Email>,
    /// 本次请求是否成功
    pub success: bool,
}

/// 重试配置
#[derive(Debug, Clone)]
pub struct RetryConfig {
    /// 最大重试次数（不含首次请求），默认 2
    pub max_retries: u32,
    /// 初始重试延迟（毫秒），默认 1000
    pub initial_delay_ms: u64,
    /// 最大重试延迟（毫秒），默认 5000
    pub max_delay_ms: u64,
    /// 请求超时（秒），默认 15
    pub timeout_secs: u64,
}

impl Default for RetryConfig {
    fn default() -> Self {
        Self {
            max_retries: 2,
            initial_delay_ms: 1000,
            max_delay_ms: 5000,
            timeout_secs: 15,
        }
    }
}

/// 创建临时邮箱的选项
#[derive(Debug, Clone, Default)]
pub struct GenerateEmailOptions {
    /// 指定渠道，不指定则随机选择
    pub channel: Option<Channel>,
    /// tempmail 渠道的有效期（分钟）
    pub duration: Option<u32>,
    /// tempmail-lol 渠道的指定域名
    pub domain: Option<String>,
    /// 重试配置
    pub retry: Option<RetryConfig>,
}

/// 获取邮件的选项
#[derive(Debug, Clone)]
pub struct GetEmailsOptions {
    /// 渠道标识（必需）
    pub channel: Channel,
    /// 邮箱地址（必需）
    pub email: String,
    /// 认证令牌（部分渠道必需）
    pub token: Option<String>,
    /// 重试配置
    pub retry: Option<RetryConfig>,
}

/// 渠道信息
#[derive(Debug, Clone)]
pub struct ChannelInfo {
    /// 渠道标识
    pub channel: Channel,
    /// 渠道显示名称
    pub name: &'static str,
    /// 对应的临时邮箱服务网站
    pub website: &'static str,
}
