/*!
 * SDK 类型定义
 */

use serde::{Deserialize, Serialize};

/// 支持的临时邮箱渠道标识
#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum Channel {
    #[serde(rename = "tempmail")]
    Tempmail,
    #[serde(rename = "tempmail-cn")]
    TempmailCn,
    #[serde(rename = "ta-easy")]
    TaEasy,
    #[serde(rename = "10minute-one")]
    TenminuteOne,
    #[serde(rename = "linshiyou")]
    Linshiyou,
    #[serde(rename = "mffac")]
    Mffac,
    #[serde(rename = "tempmail-lol")]
    TempmailLol,
    #[serde(rename = "chatgpt-org-uk")]
    ChatgptOrgUk,
    #[serde(rename = "temp-mail-io")]
    TempMailIo,
    #[serde(rename = "mail-cx")]
    MailCx,
    #[serde(rename = "catchmail")]
    Catchmail,
    #[serde(rename = "catchmail-mailistry")]
    CatchmailMailistry,
    #[serde(rename = "catchmail-zeppost")]
    CatchmailZeppost,
    #[serde(rename = "mailforspam")]
    Mailforspam,
    #[serde(rename = "mailforspam-tempmail-io")]
    MailforspamTempmailIo,
    #[serde(rename = "mailforspam-disposable")]
    MailforspamDisposable,
    #[serde(rename = "tempmailo")]
    Tempmailo,
    #[serde(rename = "tempmailc")]
    Tempmailc,
    #[serde(rename = "mailnesia")]
    Mailnesia,
    #[serde(rename = "throwawaymail")]
    Throwawaymail,
    #[serde(rename = "inboxkitten")]
    Inboxkitten,
    #[serde(rename = "getnada")]
    Getnada,
    #[serde(rename = "mail123")]
    Mail123,
    #[serde(rename = "1sec-mail")]
    OneSecMail,
    #[serde(rename = "fakemail")]
    Fakemail,
    #[serde(rename = "openinbox")]
    Openinbox,
    #[serde(rename = "inboxes")]
    Inboxes,
    #[serde(rename = "uncorreotemporal")]
    Uncorreotemporal,
    #[serde(rename = "awamail")]
    Awamail,
    #[serde(rename = "mail-tm")]
    MailTm,
    #[serde(rename = "dropmail")]
    Dropmail,
    #[serde(rename = "guerrillamail")]
    GuerrillaMail,
    #[serde(rename = "guerrillamail-com")]
    GuerrillamailCom,
    #[serde(rename = "maildrop")]
    Maildrop,
    #[serde(rename = "smail-pw")]
    SmailPw,
    #[serde(rename = "vip-215")]
    Vip215,
    #[serde(rename = "fake-legal")]
    FakeLegal,
    #[serde(rename = "moakt")]
    Moakt,
    #[serde(rename = "email10min")]
    Email10min,
    #[serde(rename = "mjj-cm")]
    MjjCm,
    #[serde(rename = "linshi-co")]
    LinshiCo,
    #[serde(rename = "harakirimail")]
    Harakirimail,
    #[serde(rename = "tempmail-plus")]
    TempmailPlus,
    #[serde(rename = "mail-gw")]
    MailGw,
    #[serde(rename = "tempmail-lol-v2")]
    TempmailLolV2,
    #[serde(rename = "sharklasers")]
    Sharklasers,
    #[serde(rename = "sharklasers-com")]
    SharklasersCom,
    #[serde(rename = "grr-la")]
    GrrLa,
    #[serde(rename = "grr-la-com")]
    GrrLaCom,
    #[serde(rename = "guerrillamail-info")]
    GuerrillamailInfo,
    #[serde(rename = "spam4me")]
    Spam4me,
    #[serde(rename = "guerrillamail-net")]
    GuerrillamailNet,
    #[serde(rename = "guerrillamail-org")]
    GuerrillamailOrg,
    #[serde(rename = "guerrillamailblock")]
    Guerrillamailblock,
    #[serde(rename = "guerrillamail-com-www")]
    GuerrillamailComWww,
}

impl std::fmt::Display for Channel {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Channel::Tempmail => write!(f, "tempmail"),
            Channel::TempmailCn => write!(f, "tempmail-cn"),
            Channel::TaEasy => write!(f, "ta-easy"),
            Channel::TenminuteOne => write!(f, "10minute-one"),
            Channel::Linshiyou => write!(f, "linshiyou"),
            Channel::Mffac => write!(f, "mffac"),
            Channel::TempmailLol => write!(f, "tempmail-lol"),
            Channel::ChatgptOrgUk => write!(f, "chatgpt-org-uk"),
            Channel::TempMailIo => write!(f, "temp-mail-io"),
            Channel::MailCx => write!(f, "mail-cx"),
            Channel::Catchmail => write!(f, "catchmail"),
            Channel::CatchmailMailistry => write!(f, "catchmail-mailistry"),
            Channel::CatchmailZeppost => write!(f, "catchmail-zeppost"),
            Channel::Mailforspam => write!(f, "mailforspam"),
            Channel::MailforspamTempmailIo => write!(f, "mailforspam-tempmail-io"),
            Channel::MailforspamDisposable => write!(f, "mailforspam-disposable"),
            Channel::Tempmailo => write!(f, "tempmailo"),
            Channel::Tempmailc => write!(f, "tempmailc"),
            Channel::Mailnesia => write!(f, "mailnesia"),
            Channel::Throwawaymail => write!(f, "throwawaymail"),
            Channel::Inboxkitten => write!(f, "inboxkitten"),
            Channel::Getnada => write!(f, "getnada"),
            Channel::Mail123 => write!(f, "mail123"),
            Channel::OneSecMail => write!(f, "1sec-mail"),
            Channel::Fakemail => write!(f, "fakemail"),
            Channel::Openinbox => write!(f, "openinbox"),
            Channel::Inboxes => write!(f, "inboxes"),
            Channel::Uncorreotemporal => write!(f, "uncorreotemporal"),
            Channel::Awamail => write!(f, "awamail"),
            Channel::MailTm => write!(f, "mail-tm"),
            Channel::Dropmail => write!(f, "dropmail"),
            Channel::GuerrillaMail => write!(f, "guerrillamail"),
            Channel::GuerrillamailCom => write!(f, "guerrillamail-com"),
            Channel::Maildrop => write!(f, "maildrop"),
            Channel::SmailPw => write!(f, "smail-pw"),
            Channel::Vip215 => write!(f, "vip-215"),
            Channel::FakeLegal => write!(f, "fake-legal"),
            Channel::Moakt => write!(f, "moakt"),
            Channel::Email10min => write!(f, "email10min"),
            Channel::MjjCm => write!(f, "mjj-cm"),
            Channel::LinshiCo => write!(f, "linshi-co"),
            Channel::Harakirimail => write!(f, "harakirimail"),
            Channel::TempmailPlus => write!(f, "tempmail-plus"),
            Channel::MailGw => write!(f, "mail-gw"),
            Channel::TempmailLolV2 => write!(f, "tempmail-lol-v2"),
            Channel::Sharklasers => write!(f, "sharklasers"),
            Channel::SharklasersCom => write!(f, "sharklasers-com"),
            Channel::GrrLa => write!(f, "grr-la"),
            Channel::GrrLaCom => write!(f, "grr-la-com"),
            Channel::GuerrillamailInfo => write!(f, "guerrillamail-info"),
            Channel::Spam4me => write!(f, "spam4me"),
            Channel::GuerrillamailNet => write!(f, "guerrillamail-net"),
            Channel::GuerrillamailOrg => write!(f, "guerrillamail-org"),
            Channel::Guerrillamailblock => write!(f, "guerrillamailblock"),
            Channel::GuerrillamailComWww => write!(f, "guerrillamail-com-www"),
        }
    }
}

/// 创建临时邮箱后返回的邮箱信息
/// Token 等认证信息由 SDK 内部维护，不对外暴露
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EmailInfo {
    /// 创建该邮箱所使用的渠道
    pub channel: Channel,
    /// 临时邮箱地址
    pub email: String,
    /// 认证令牌，由 SDK 内部维护，不对外暴露
    #[serde(skip_serializing)]
    pub(crate) token: Option<String>,
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
    /// 指定邮箱域名/接入域名（如 tempmail-cn、tempmail-lol、catchmail、mailforspam）
    pub domain: Option<String>,
    /// 重试配置
    pub retry: Option<RetryConfig>,
}

/// 获取邮件的选项
/// Channel/Email/Token 等由 SDK 从 EmailInfo 中自动获取，用户无需手动传递
#[derive(Debug, Clone, Default)]
pub struct GetEmailsOptions {
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
