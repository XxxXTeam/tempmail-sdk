/*!
 * SDK 主入口
 * 聚合所有渠道的逻辑，提供统一的 API
 */

use rand::Rng;
use crate::types::*;
use crate::retry::with_retry;
use crate::providers;

/// 所有支持的渠道列表
pub const ALL_CHANNELS: &[Channel] = &[
    Channel::Tempmail, Channel::LinshiEmail, Channel::TempmailLol,
    Channel::ChatgptOrgUk, Channel::TempmailLa, Channel::TempMailIO,
    Channel::Awamail, Channel::MailTm, Channel::Dropmail,
    Channel::GuerrillaMail, Channel::Maildrop,
];

/// 获取所有支持的渠道信息列表
pub fn list_channels() -> Vec<ChannelInfo> {
    ALL_CHANNELS.iter().map(|ch| get_channel_info(ch).unwrap()).collect()
}

/// 获取指定渠道的详细信息
pub fn get_channel_info(channel: &Channel) -> Option<ChannelInfo> {
    Some(match channel {
        Channel::Tempmail => ChannelInfo { channel: Channel::Tempmail, name: "TempMail", website: "tempmail.ing" },
        Channel::LinshiEmail => ChannelInfo { channel: Channel::LinshiEmail, name: "临时邮箱", website: "linshi-email.com" },
        Channel::TempmailLol => ChannelInfo { channel: Channel::TempmailLol, name: "TempMail LOL", website: "tempmail.lol" },
        Channel::ChatgptOrgUk => ChannelInfo { channel: Channel::ChatgptOrgUk, name: "ChatGPT Mail", website: "mail.chatgpt.org.uk" },
        Channel::TempmailLa => ChannelInfo { channel: Channel::TempmailLa, name: "TempMail LA", website: "tempmail.la" },
        Channel::TempMailIO => ChannelInfo { channel: Channel::TempMailIO, name: "Temp Mail IO", website: "temp-mail.io" },
        Channel::Awamail => ChannelInfo { channel: Channel::Awamail, name: "AwaMail", website: "awamail.com" },
        Channel::MailTm => ChannelInfo { channel: Channel::MailTm, name: "Mail.tm", website: "mail.tm" },
        Channel::Dropmail => ChannelInfo { channel: Channel::Dropmail, name: "DropMail", website: "dropmail.me" },
        Channel::GuerrillaMail => ChannelInfo { channel: Channel::GuerrillaMail, name: "Guerrilla Mail", website: "guerrillamail.com" },
        Channel::Maildrop => ChannelInfo { channel: Channel::Maildrop, name: "Maildrop", website: "maildrop.cc" },
    })
}

/// 创建临时邮箱
///
/// 错误处理策略:
/// - 网络错误、超时、HTTP 4xx/5xx → 自动重试（默认 2 次，指数退避）
/// - 重试耗尽后仍失败时返回 Err
pub fn generate_email(options: &GenerateEmailOptions) -> Result<EmailInfo, String> {
    let channel = options.channel.clone().unwrap_or_else(|| {
        let mut rng = rand::thread_rng();
        ALL_CHANNELS[rng.gen_range(0..ALL_CHANNELS.len())].clone()
    });

    log::info!("创建临时邮箱, 渠道: {}", channel);

    let ch = channel.clone();
    let dur = options.duration.unwrap_or(30);
    let dom = options.domain.clone();

    let result = with_retry(|| {
        generate_email_once(&ch, dur, dom.as_deref())
    }, options.retry.as_ref())?;

    log::info!("邮箱创建成功: {}", result.email);
    Ok(result)
}

fn generate_email_once(channel: &Channel, duration: u32, domain: Option<&str>) -> Result<EmailInfo, String> {
    match channel {
        Channel::Tempmail => providers::tempmail::generate_email(duration),
        Channel::LinshiEmail => providers::linshi_email::generate_email(),
        Channel::TempmailLol => providers::tempmail_lol::generate_email(domain),
        Channel::ChatgptOrgUk => providers::chatgpt_org_uk::generate_email(),
        Channel::TempmailLa => providers::tempmail_la::generate_email(),
        Channel::TempMailIO => providers::temp_mail_io::generate_email(),
        Channel::Awamail => providers::awamail::generate_email(),
        Channel::MailTm => providers::mail_tm::generate_email(),
        Channel::Dropmail => providers::dropmail::generate_email(),
        Channel::GuerrillaMail => providers::guerrillamail::generate_email(),
        Channel::Maildrop => providers::maildrop::generate_email(),
    }
}

/// 获取邮件列表
///
/// 错误处理策略:
/// - 网络错误、超时、HTTP 4xx/5xx → 自动重试（默认 2 次）
/// - 重试耗尽后返回 GetEmailsResult { success: false, emails: [] }
pub fn get_emails(options: &GetEmailsOptions) -> GetEmailsResult {
    let channel = options.channel.clone();
    let email = options.email.clone();
    let token = options.token.clone();

    log::debug!("获取邮件, 渠道: {}, 邮箱: {}", channel, email);

    let ch = channel.clone();
    let em = email.clone();
    let tk = token.clone();

    match with_retry(|| {
        get_emails_once(&ch, &em, tk.as_deref())
    }, options.retry.as_ref()) {
        Ok(emails) => {
            if !emails.is_empty() {
                log::info!("获取到 {} 封邮件, 渠道: {}", emails.len(), channel);
            } else {
                log::debug!("暂无邮件, 渠道: {}", channel);
            }
            GetEmailsResult { channel, email, emails, success: true }
        }
        Err(e) => {
            log::error!("获取邮件失败, 渠道: {}, 错误: {}", channel, e);
            GetEmailsResult { channel, email, emails: vec![], success: false }
        }
    }
}

fn get_emails_once(channel: &Channel, email: &str, token: Option<&str>) -> Result<Vec<Email>, String> {
    match channel {
        Channel::Tempmail => providers::tempmail::get_emails(email),
        Channel::LinshiEmail => providers::linshi_email::get_emails(email),
        Channel::TempmailLol => {
            let t = token.ok_or("token is required for tempmail-lol")?;
            providers::tempmail_lol::get_emails(t, email)
        }
        Channel::ChatgptOrgUk => providers::chatgpt_org_uk::get_emails(email),
        Channel::TempmailLa => providers::tempmail_la::get_emails(email),
        Channel::TempMailIO => providers::temp_mail_io::get_emails(email),
        Channel::Awamail => {
            let t = token.ok_or("token is required for awamail")?;
            providers::awamail::get_emails(t, email)
        }
        Channel::MailTm => {
            let t = token.ok_or("token is required for mail-tm")?;
            providers::mail_tm::get_emails(t, email)
        }
        Channel::Dropmail => {
            let t = token.ok_or("token is required for dropmail")?;
            providers::dropmail::get_emails(t, email)
        }
        Channel::GuerrillaMail => {
            let t = token.ok_or("token is required for guerrillamail")?;
            providers::guerrillamail::get_emails(t, email)
        }
        Channel::Maildrop => {
            let t = token.ok_or("token is required for maildrop")?;
            providers::maildrop::get_emails(t, email)
        }
    }
}

/// 临时邮箱客户端
pub struct TempEmailClient {
    email_info: Option<EmailInfo>,
}

impl TempEmailClient {
    pub fn new() -> Self {
        Self { email_info: None }
    }

    /// 创建临时邮箱并缓存邮箱信息
    pub fn generate(&mut self, options: &GenerateEmailOptions) -> Result<&EmailInfo, String> {
        let info = generate_email(options)?;
        self.email_info = Some(info);
        Ok(self.email_info.as_ref().unwrap())
    }

    /// 获取当前邮箱的邮件列表
    pub fn get_emails(&self) -> Result<GetEmailsResult, String> {
        let info = self.email_info.as_ref().ok_or("No email generated. Call generate() first")?;
        Ok(get_emails(&GetEmailsOptions {
            channel: info.channel.clone(),
            email: info.email.clone(),
            token: info.token.clone(),
            retry: None,
        }))
    }

    /// 获取当前缓存的邮箱信息
    pub fn email_info(&self) -> Option<&EmailInfo> {
        self.email_info.as_ref()
    }
}
