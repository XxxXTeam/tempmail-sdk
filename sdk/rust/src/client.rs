/*!
 * SDK 主入口
 * 聚合所有渠道的逻辑，提供统一的 API
 */

use crate::registry::all_channels;
use crate::retry::with_retry_with_attempts;
use crate::telemetry::report_telemetry;
use crate::types::*;

/// 创建临时邮箱
///
/// 错误处理策略:
/// - 指定渠道失败时，自动尝试其他可用渠道（打乱顺序逐个尝试）
/// - 未指定渠道时，打乱全部渠道逐个尝试，直到成功
/// - 所有渠道均不可用时返回 None（不返回 Err）
pub fn generate_email(options: &GenerateEmailOptions) -> Option<EmailInfo> {
    use crate::backend_groups::{
        get_backend, is_backend_open, record_backend_failure, record_backend_success,
    };
    use crate::channel_domains::filter_channels_by_domain;
    use std::collections::HashSet;
    use std::time::Instant;

    let try_order = build_channel_order(options.channel.as_ref());

    // 解析域名筛选条件
    let mut target_domains: Vec<String> = Vec::new();
    if let Some(ref suffix) = options.suffix {
        let s = suffix.strip_prefix('@').unwrap_or(suffix);
        target_domains.push(s.to_string());
    }
    if let Some(ref domains) = options.domains {
        target_domains.extend(domains.clone());
    }
    let try_order = if target_domains.is_empty() {
        try_order
    } else {
        filter_channels_by_domain(&try_order, &target_domains)
    };

    let dur = options.duration.unwrap_or(30);
    let dom = options.domain.clone();
    let max_channels = options.max_channels_tried.unwrap_or(20);
    let total_timeout_secs = options.total_timeout.unwrap_or(60.0);
    let start = Instant::now();

    let mut channels_tried: u32 = 0;
    let mut last_err = String::new();
    let mut failed_backends: HashSet<&'static str> = HashSet::new();

    for ch in &try_order {
        if channels_tried >= max_channels {
            log::warn!("已达最大尝试渠道数 {}，停止尝试", max_channels);
            break;
        }
        if start.elapsed().as_secs_f64() >= total_timeout_secs {
            log::warn!("已超过整体超时 {:.0}s，停止尝试", total_timeout_secs);
            break;
        }

        if let Some(backend) = get_backend(ch) {
            if failed_backends.contains(backend) {
                log::debug!("跳过渠道 {}（同组后端 {} 本次已失败）", ch, backend);
                continue;
            }
            if !is_backend_open(backend) {
                log::debug!("跳过渠道 {}（后端 {} 处于熔断冷却中）", ch, backend);
                continue;
            }
        }

        channels_tried += 1;
        log::info!("创建临时邮箱, 渠道: {}", ch);
        let c = ch.clone();
        let d = dom.clone();
        match with_retry_with_attempts(
            || generate_email_once(&c, dur, d.as_deref()),
            options.retry.as_ref(),
        ) {
            Ok((result, attempts)) => {
                log::info!("邮箱创建成功: {} (渠道: {})", result.email, ch);
                if let Some(backend) = get_backend(ch) {
                    record_backend_success(backend);
                }
                report_telemetry(
                    "generate_email",
                    &ch.to_string(),
                    true,
                    attempts,
                    channels_tried,
                    "",
                );
                return Some(result);
            }
            Err((e, _)) => {
                last_err = e.clone();
                log::warn!("渠道 {} 不可用: {}，尝试下一个渠道", ch, e);
                if let Some(backend) = get_backend(ch) {
                    failed_backends.insert(backend);
                    record_backend_failure(backend);
                }
            }
        }
    }

    log::error!("所有渠道均不可用，创建邮箱失败");
    report_telemetry("generate_email", "", false, 0, channels_tried, &last_err);
    None
}

/// 构建渠道尝试顺序
/// 指定渠道时优先尝试该渠道，其余渠道打乱追加
/// 未指定时打乱全部渠道
/// 返回值是本端运行时随机顺序，不作为跨 SDK 一致性约束
fn build_channel_order(preferred: Option<&Channel>) -> Vec<Channel> {
    use rand::seq::SliceRandom;
    let mut shuffled: Vec<Channel> = all_channels().to_vec();
    shuffled.shuffle(&mut rand::thread_rng());
    match preferred {
        Some(pref) => {
            let rest: Vec<Channel> = shuffled.into_iter().filter(|ch| ch != pref).collect();
            let mut result = vec![pref.clone()];
            result.extend(rest);
            result
        }
        None => shuffled,
    }
}

/// 创建邮箱分发：委托注册表中该渠道的 generate 闭包（对应重构前的大 match）
fn generate_email_once(
    channel: &Channel,
    duration: u32,
    domain: Option<&str>,
) -> Result<EmailInfo, String> {
    match crate::registry::spec_for(channel) {
        Some(spec) => (spec.generate)(duration, domain),
        None => Err(format!("unsupported channel: {}", channel)),
    }
}

/// 获取邮件列表
/// Channel/Email/Token 等由 SDK 从 EmailInfo 中自动获取，用户无需手动传递
///
/// 错误处理策略:
/// - 网络错误、超时、429、HTTP 5xx → 自动重试（默认 2 次）
/// - 重试耗尽后返回 GetEmailsResult { success: false, emails: [] }
pub fn get_emails(info: &EmailInfo, options: Option<&GetEmailsOptions>) -> GetEmailsResult {
    let channel = info.channel.clone();
    let email = info.email.clone();
    let token = info.token.clone();
    let retry = options.and_then(|o| o.retry.as_ref());

    if email.is_empty() && channel != Channel::TempmailLol {
        report_telemetry(
            "get_emails",
            &channel.to_string(),
            false,
            0,
            0,
            "email is required",
        );
        return GetEmailsResult {
            channel,
            email,
            emails: vec![],
            success: false,
        };
    }

    log::debug!("获取邮件, 渠道: {}, 邮箱: {}", channel, email);

    let ch = channel.clone();
    let em = email.clone();
    let tk = token.clone();

    match with_retry_with_attempts(|| get_emails_once(&ch, &em, tk.as_deref()), retry) {
        Ok((emails, attempts)) => {
            if !emails.is_empty() {
                log::info!("获取到 {} 封邮件, 渠道: {}", emails.len(), channel);
            } else {
                log::debug!("暂无邮件, 渠道: {}", channel);
            }
            report_telemetry("get_emails", &channel.to_string(), true, attempts, 0, "");
            GetEmailsResult {
                channel,
                email,
                emails,
                success: true,
            }
        }
        Err((e, attempts)) => {
            log::error!("获取邮件失败, 渠道: {}, 错误: {}", channel, e);
            report_telemetry("get_emails", &channel.to_string(), false, attempts, 0, &e);
            GetEmailsResult {
                channel,
                email,
                emails: vec![],
                success: false,
            }
        }
    }
}

/// 收信分发：委托注册表中该渠道的 get_emails 闭包（对应重构前的大 match）
fn get_emails_once(
    channel: &Channel,
    email: &str,
    token: Option<&str>,
) -> Result<Vec<Email>, String> {
    match crate::registry::spec_for(channel) {
        Some(spec) => (spec.get_emails)(email, token),
        None => Err(format!("unsupported channel: {}", channel)),
    }
}

/// 临时邮箱客户端
#[derive(Default)]
pub struct TempEmailClient {
    email_info: Option<EmailInfo>,
}

impl TempEmailClient {
    pub fn new() -> Self {
        Self { email_info: None }
    }

    /// 创建临时邮箱并缓存邮箱信息
    /// 所有渠道均不可用时返回 None
    pub fn generate(&mut self, options: &GenerateEmailOptions) -> Option<&EmailInfo> {
        let info = generate_email(options)?;
        self.email_info = Some(info);
        self.email_info.as_ref()
    }

    /// 获取当前邮箱的邮件列表
    pub fn get_emails(
        &self,
        options: Option<&GetEmailsOptions>,
    ) -> Result<GetEmailsResult, String> {
        let info = self
            .email_info
            .as_ref()
            .ok_or("No email generated. Call generate() first")?;
        Ok(get_emails(info, options))
    }

    /// 获取当前缓存的邮箱信息
    pub fn email_info(&self) -> Option<&EmailInfo> {
        self.email_info.as_ref()
    }
}
