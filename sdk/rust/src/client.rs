/*!
 * SDK 主入口
 * 聚合所有渠道的逻辑，提供统一的 API
 */

use crate::providers;
use crate::retry::with_retry_with_attempts;
use crate::telemetry::report_telemetry;
use crate::types::*;

/// 所有支持的渠道列表
pub const ALL_CHANNELS: &[Channel] = &[
    Channel::Tempmail,
    Channel::TempmailCn,
    Channel::TaEasy,
    Channel::TenminuteOne,
    Channel::Linshiyou,
    Channel::Mffac,
    Channel::TempmailLol,
    Channel::ChatgptOrgUk,
    Channel::TempMailIo,
    Channel::MailCx,
    Channel::Catchmail,
    Channel::CatchmailMailistry,
    Channel::CatchmailZeppost,
    Channel::Mailforspam,
    Channel::MailforspamTempmailIo,
    Channel::MailforspamDisposable,
    Channel::Tempmailo,
    Channel::Tempmailc,
    Channel::Mailnesia,
    Channel::Throwawaymail,
    Channel::Inboxkitten,
    Channel::Getnada,
    Channel::Mail123,
    Channel::OneSecMail,
    Channel::Fakemail,
    Channel::Openinbox,
    Channel::Inboxes,
    Channel::Uncorreotemporal,
    Channel::Awamail,
    Channel::MailTm,
    Channel::Dropmail,
    Channel::GuerrillaMail,
    Channel::GuerrillamailCom,
    Channel::Maildrop,
    Channel::SmailPw,
    Channel::Vip215,
    Channel::FakeLegal,
    Channel::Moakt,
    Channel::Email10min,
    Channel::MjjCm,
    Channel::LinshiCo,
    Channel::Harakirimail,
    Channel::TempmailPlus,
    Channel::MailGw,
    Channel::TempmailLolV2,
    Channel::Sharklasers,
    Channel::SharklasersCom,
    Channel::GrrLa,
    Channel::GrrLaCom,
    Channel::GuerrillamailInfo,
    Channel::Spam4me,
    Channel::GuerrillamailNet,
    Channel::GuerrillamailOrg,
    Channel::Guerrillamailblock,
    Channel::GuerrillamailComWww,
];

/// 获取所有支持的渠道信息列表
pub fn list_channels() -> Vec<ChannelInfo> {
    ALL_CHANNELS
        .iter()
        .map(|ch| get_channel_info(ch).unwrap())
        .collect()
}

/// 获取指定渠道的详细信息
pub fn get_channel_info(channel: &Channel) -> Option<ChannelInfo> {
    Some(match channel {
        Channel::Tempmail => ChannelInfo {
            channel: Channel::Tempmail,
            name: "TempMail",
            website: "tempmail.ing",
        },
        Channel::TempmailCn => ChannelInfo {
            channel: Channel::TempmailCn,
            name: "TempMail CN",
            website: "tempmail.cn",
        },
        Channel::TaEasy => ChannelInfo {
            channel: Channel::TaEasy,
            name: "TA Easy",
            website: "ta-easy.com",
        },
        Channel::TenminuteOne => ChannelInfo {
            channel: Channel::TenminuteOne,
            name: "10 Minute Email",
            website: "10minutemail.one",
        },
        Channel::Linshiyou => ChannelInfo {
            channel: Channel::Linshiyou,
            name: "临时邮",
            website: "linshiyou.com",
        },
        Channel::Mffac => ChannelInfo {
            channel: Channel::Mffac,
            name: "MFFAC",
            website: "mffac.com",
        },
        Channel::TempmailLol => ChannelInfo {
            channel: Channel::TempmailLol,
            name: "TempMail LOL",
            website: "tempmail.lol",
        },
        Channel::ChatgptOrgUk => ChannelInfo {
            channel: Channel::ChatgptOrgUk,
            name: "ChatGPT Mail",
            website: "mail.chatgpt.org.uk",
        },
        Channel::TempMailIo => ChannelInfo {
            channel: Channel::TempMailIo,
            name: "Temp-Mail.io",
            website: "temp-mail.io",
        },
        Channel::MailCx => ChannelInfo {
            channel: Channel::MailCx,
            name: "Mail.cx",
            website: "mail.cx",
        },
        Channel::Catchmail => ChannelInfo {
            channel: Channel::Catchmail,
            name: "Catchmail",
            website: "catchmail.io",
        },
        Channel::CatchmailMailistry => ChannelInfo {
            channel: Channel::CatchmailMailistry,
            name: "Catchmail Mailistry",
            website: "mailistry.com",
        },
        Channel::CatchmailZeppost => ChannelInfo {
            channel: Channel::CatchmailZeppost,
            name: "Catchmail Zeppost",
            website: "zeppost.com",
        },
        Channel::Mailforspam => ChannelInfo {
            channel: Channel::Mailforspam,
            name: "MailForSpam",
            website: "mailforspam.com",
        },
        Channel::MailforspamTempmailIo => ChannelInfo {
            channel: Channel::MailforspamTempmailIo,
            name: "MailForSpam TempMail.io",
            website: "tempmail.io",
        },
        Channel::MailforspamDisposable => ChannelInfo {
            channel: Channel::MailforspamDisposable,
            name: "MailForSpam Disposable",
            website: "disposable.email",
        },
        Channel::Tempmailo => ChannelInfo {
            channel: Channel::Tempmailo,
            name: "Tempmailo",
            website: "tempmailo.com",
        },
        Channel::Tempmailc => ChannelInfo {
            channel: Channel::Tempmailc,
            name: "TempMailC",
            website: "tempmailc.com",
        },
        Channel::Mailnesia => ChannelInfo {
            channel: Channel::Mailnesia,
            name: "Mailnesia",
            website: "mailnesia.com",
        },
        Channel::Throwawaymail => ChannelInfo {
            channel: Channel::Throwawaymail,
            name: "ThrowawayMail",
            website: "throwawaymail.app",
        },
        Channel::Inboxkitten => ChannelInfo {
            channel: Channel::Inboxkitten,
            name: "InboxKitten",
            website: "inboxkitten.com",
        },
        Channel::Getnada => ChannelInfo {
            channel: Channel::Getnada,
            name: "GetNada",
            website: "getnada.net",
        },
        Channel::Mail123 => ChannelInfo {
            channel: Channel::Mail123,
            name: "Mail123",
            website: "mail123.fr",
        },
        Channel::OneSecMail => ChannelInfo {
            channel: Channel::OneSecMail,
            name: "1SecMail",
            website: "1sec-mail.com",
        },
        Channel::Fakemail => ChannelInfo {
            channel: Channel::Fakemail,
            name: "FakeMail",
            website: "fakemail.net",
        },
        Channel::Openinbox => ChannelInfo {
            channel: Channel::Openinbox,
            name: "OpenInbox",
            website: "openinbox.io",
        },
        Channel::Inboxes => ChannelInfo {
            channel: Channel::Inboxes,
            name: "Inboxes",
            website: "inboxes.com",
        },
        Channel::Uncorreotemporal => ChannelInfo {
            channel: Channel::Uncorreotemporal,
            name: "UnCorreoTemporal",
            website: "uncorreotemporal.com",
        },
        Channel::Awamail => ChannelInfo {
            channel: Channel::Awamail,
            name: "AwaMail",
            website: "awamail.com",
        },
        Channel::MailTm => ChannelInfo {
            channel: Channel::MailTm,
            name: "Mail.tm",
            website: "mail.tm",
        },
        Channel::Dropmail => ChannelInfo {
            channel: Channel::Dropmail,
            name: "DropMail",
            website: "dropmail.me",
        },
        Channel::GuerrillaMail => ChannelInfo {
            channel: Channel::GuerrillaMail,
            name: "Guerrilla Mail",
            website: "guerrillamail.com",
        },
        Channel::GuerrillamailCom => ChannelInfo {
            channel: Channel::GuerrillamailCom,
            name: "GuerrillaMail Root",
            website: "guerrillamail.com",
        },
        Channel::Maildrop => ChannelInfo {
            channel: Channel::Maildrop,
            name: "Maildrop",
            website: "maildrop.cx",
        },
        Channel::SmailPw => ChannelInfo {
            channel: Channel::SmailPw,
            name: "Smail.pw",
            website: "smail.pw",
        },
        Channel::Vip215 => ChannelInfo {
            channel: Channel::Vip215,
            name: "VIP 215",
            website: "vip.215.im",
        },
        Channel::FakeLegal => ChannelInfo {
            channel: Channel::FakeLegal,
            name: "Fake Legal",
            website: "fake.legal",
        },
        Channel::Moakt => ChannelInfo {
            channel: Channel::Moakt,
            name: "Moakt",
            website: "moakt.com",
        },
        Channel::Email10min => ChannelInfo {
            channel: Channel::Email10min,
            name: "Email10Min",
            website: "email10min.com",
        },
        Channel::MjjCm => ChannelInfo {
            channel: Channel::MjjCm,
            name: "MJJ Mail",
            website: "mjj.cm",
        },
        Channel::LinshiCo => ChannelInfo {
            channel: Channel::LinshiCo,
            name: "临时Co",
            website: "linshi.co",
        },
        Channel::Harakirimail => ChannelInfo {
            channel: Channel::Harakirimail,
            name: "HarakiriMail",
            website: "harakirimail.com",
        },
        Channel::TempmailPlus => ChannelInfo {
            channel: Channel::TempmailPlus,
            name: "TempMail Plus",
            website: "tempmail.plus",
        },
        Channel::MailGw => ChannelInfo {
            channel: Channel::MailGw,
            name: "Mail.gw",
            website: "mail.gw",
        },
        Channel::TempmailLolV2 => ChannelInfo {
            channel: Channel::TempmailLolV2,
            name: "TempMail LOL V2",
            website: "tempmail.lol",
        },
        Channel::Sharklasers => ChannelInfo {
            channel: Channel::Sharklasers,
            name: "SharkLasers",
            website: "sharklasers.com",
        },
        Channel::SharklasersCom => ChannelInfo {
            channel: Channel::SharklasersCom,
            name: "SharkLasers Root",
            website: "sharklasers.com",
        },
        Channel::GrrLa => ChannelInfo {
            channel: Channel::GrrLa,
            name: "Grr.la",
            website: "grr.la",
        },
        Channel::GrrLaCom => ChannelInfo {
            channel: Channel::GrrLaCom,
            name: "Grr.la Root",
            website: "grr.la",
        },
        Channel::GuerrillamailInfo => ChannelInfo {
            channel: Channel::GuerrillamailInfo,
            name: "GuerrillaMail Info",
            website: "guerrillamail.info",
        },
        Channel::Spam4me => ChannelInfo {
            channel: Channel::Spam4me,
            name: "Spam4.me",
            website: "spam4.me",
        },
        Channel::GuerrillamailNet => ChannelInfo {
            channel: Channel::GuerrillamailNet,
            name: "GuerrillaMail Net",
            website: "guerrillamail.net",
        },
        Channel::GuerrillamailOrg => ChannelInfo {
            channel: Channel::GuerrillamailOrg,
            name: "GuerrillaMail Org",
            website: "guerrillamail.org",
        },
        Channel::Guerrillamailblock => ChannelInfo {
            channel: Channel::Guerrillamailblock,
            name: "GuerrillaMailBlock",
            website: "guerrillamailblock.com",
        },
        Channel::GuerrillamailComWww => ChannelInfo {
            channel: Channel::GuerrillamailComWww,
            name: "GuerrillaMail WWW",
            website: "guerrillamail.com",
        },
    })
}

/// 创建临时邮箱
///
/// 错误处理策略:
/// - 指定渠道失败时，自动尝试其他可用渠道（打乱顺序逐个尝试）
/// - 未指定渠道时，打乱全部渠道逐个尝试，直到成功
/// - 所有渠道均不可用时返回 None（不返回 Err）
pub fn generate_email(options: &GenerateEmailOptions) -> Option<EmailInfo> {
    let try_order = build_channel_order(options.channel.as_ref());
    let dur = options.duration.unwrap_or(30);
    let dom = options.domain.clone();

    let mut channels_tried: u32 = 0;
    let mut last_err = String::new();
    for ch in &try_order {
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
fn build_channel_order(preferred: Option<&Channel>) -> Vec<Channel> {
    use rand::seq::SliceRandom;
    let mut shuffled: Vec<Channel> = ALL_CHANNELS.to_vec();
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

fn with_channel(mut info: EmailInfo, channel: Channel) -> EmailInfo {
    info.channel = channel;
    info
}

fn generate_email_once(
    channel: &Channel,
    duration: u32,
    domain: Option<&str>,
) -> Result<EmailInfo, String> {
    match channel {
        Channel::Tempmail => providers::tempmail::generate_email(duration),
        Channel::TempmailCn => providers::tempmail_cn::generate_email(domain),
        Channel::TaEasy => providers::ta_easy::generate_email(),
        Channel::TenminuteOne => providers::tenminute_one::generate_email(domain),
        Channel::Linshiyou => providers::linshiyou::generate_email(),
        Channel::Mffac => providers::mffac::generate_email(),
        Channel::TempmailLol => providers::tempmail_lol::generate_email(domain),
        Channel::ChatgptOrgUk => providers::chatgpt_org_uk::generate_email(),
        Channel::TempMailIo => providers::temp_mail_io::generate_email(),
        Channel::MailCx => providers::mail_cx::generate_email(domain),
        Channel::Catchmail => providers::catchmail::generate_email(domain),
        Channel::CatchmailMailistry => providers::catchmail::generate_email(Some("mailistry.com"))
            .map(|info| with_channel(info, Channel::CatchmailMailistry)),
        Channel::CatchmailZeppost => providers::catchmail::generate_email(Some("zeppost.com"))
            .map(|info| with_channel(info, Channel::CatchmailZeppost)),
        Channel::Mailforspam => providers::mailforspam::generate_email(domain),
        Channel::MailforspamTempmailIo => {
            providers::mailforspam::generate_email(Some("tempmail.io"))
                .map(|info| with_channel(info, Channel::MailforspamTempmailIo))
        }
        Channel::MailforspamDisposable => {
            providers::mailforspam::generate_email(Some("disposable.email"))
                .map(|info| with_channel(info, Channel::MailforspamDisposable))
        }
        Channel::Tempmailo => providers::tempmailo::generate_email(),
        Channel::Tempmailc => providers::tempmailc::generate_email(),
        Channel::Mailnesia => providers::mailnesia::generate_email(),
        Channel::Throwawaymail => providers::throwawaymail::generate_email(),
        Channel::Inboxkitten => providers::inboxkitten::generate_email(),
        Channel::Getnada => providers::getnada::generate_email(),
        Channel::Mail123 => providers::mail123::generate_email(),
        Channel::OneSecMail => providers::one_sec_mail::generate_email(),
        Channel::Fakemail => providers::fakemail::generate_email(),
        Channel::Openinbox => providers::openinbox::generate_email(),
        Channel::Inboxes => providers::inboxes::generate_email(domain),
        Channel::Uncorreotemporal => providers::uncorreotemporal::generate_email(),
        Channel::Awamail => providers::awamail::generate_email(),
        Channel::MailTm => providers::mail_tm::generate_email(),
        Channel::Dropmail => providers::dropmail::generate_email(),
        Channel::GuerrillaMail => providers::guerrillamail::generate_email(),
        Channel::GuerrillamailCom => {
            providers::guerrillamail_mirrors::guerrillamail_com::generate_email()
        }
        Channel::Maildrop => providers::maildrop::generate_email(domain),
        Channel::SmailPw => providers::smail_pw::generate_email(),
        Channel::Vip215 => providers::vip_215::generate_email(),
        Channel::FakeLegal => providers::fake_legal::generate_email(domain),
        Channel::Moakt => providers::moakt::generate_email(domain),
        Channel::Email10min => providers::email10min::generate_email(),
        Channel::MjjCm => providers::socketio_mail::mjj_cm::generate_email(),
        Channel::LinshiCo => providers::socketio_mail::linshi_co::generate_email(),
        Channel::Harakirimail => providers::harakirimail::generate_email(),
        Channel::TempmailPlus => providers::tempmail_plus::generate_email(),
        Channel::MailGw => providers::mail_gw::generate_email(),
        Channel::TempmailLolV2 => providers::tempmail_lol_v2::generate_email(),
        Channel::Sharklasers => providers::guerrillamail_mirrors::sharklasers::generate_email(),
        Channel::SharklasersCom => {
            providers::guerrillamail_mirrors::sharklasers_com::generate_email()
        }
        Channel::GrrLa => providers::guerrillamail_mirrors::grr_la::generate_email(),
        Channel::GrrLaCom => providers::guerrillamail_mirrors::grr_la_com::generate_email(),
        Channel::GuerrillamailInfo => {
            providers::guerrillamail_mirrors::guerrillamail_info::generate_email()
        }
        Channel::Spam4me => providers::guerrillamail_mirrors::spam4me::generate_email(),
        Channel::GuerrillamailNet => {
            providers::guerrillamail_mirrors::guerrillamail_net::generate_email()
        }
        Channel::GuerrillamailOrg => {
            providers::guerrillamail_mirrors::guerrillamail_org::generate_email()
        }
        Channel::Guerrillamailblock => {
            providers::guerrillamail_mirrors::guerrillamailblock::generate_email()
        }
        Channel::GuerrillamailComWww => {
            providers::guerrillamail_mirrors::guerrillamail_com_www::generate_email()
        }
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

fn get_emails_once(
    channel: &Channel,
    email: &str,
    token: Option<&str>,
) -> Result<Vec<Email>, String> {
    match channel {
        Channel::Tempmail => providers::tempmail::get_emails(email),
        Channel::TempmailCn => providers::tempmail_cn::get_emails(email),
        Channel::TaEasy => {
            let t = token.ok_or("token is required for ta-easy")?;
            providers::ta_easy::get_emails(email, t)
        }
        Channel::TenminuteOne => {
            let t = token.ok_or("token is required for 10minute-one")?;
            providers::tenminute_one::get_emails(t, email)
        }
        Channel::Linshiyou => {
            let t = token.ok_or("token is required for linshiyou")?;
            providers::linshiyou::get_emails(t, email)
        }
        Channel::Mffac => providers::mffac::get_emails(email, token),
        Channel::TempmailLol => {
            let t = token.ok_or("token is required for tempmail-lol")?;
            providers::tempmail_lol::get_emails(t, email)
        }
        Channel::ChatgptOrgUk => {
            let t = token.ok_or("token is required for chatgpt-org-uk")?;
            providers::chatgpt_org_uk::get_emails(t, email)
        }
        Channel::TempMailIo => providers::temp_mail_io::get_emails(email),
        Channel::MailCx => {
            let t = token.ok_or("token is required for mail-cx")?;
            providers::mail_cx::get_emails(t, email)
        }
        Channel::Catchmail => providers::catchmail::get_emails(email),
        Channel::CatchmailMailistry => providers::catchmail::get_emails(email),
        Channel::CatchmailZeppost => providers::catchmail::get_emails(email),
        Channel::Mailforspam => providers::mailforspam::get_emails(email),
        Channel::MailforspamTempmailIo => providers::mailforspam::get_emails(email),
        Channel::MailforspamDisposable => providers::mailforspam::get_emails(email),
        Channel::Tempmailo => {
            let t = token.ok_or("token is required for tempmailo")?;
            providers::tempmailo::get_emails(t, email)
        }
        Channel::Tempmailc => providers::tempmailc::get_emails(email),
        Channel::Mailnesia => providers::mailnesia::get_emails(email),
        Channel::Throwawaymail => {
            let t = token.ok_or("token is required for throwawaymail")?;
            providers::throwawaymail::get_emails(t, email)
        }
        Channel::Inboxkitten => providers::inboxkitten::get_emails(email),
        Channel::Getnada => {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }
        Channel::Mail123 => providers::mail123::get_emails(email),
        Channel::OneSecMail => {
            let t = token.ok_or("token is required for 1sec-mail")?;
            providers::one_sec_mail::get_emails(t, email)
        }
        Channel::Fakemail => {
            let t = token.ok_or("token is required for fakemail")?;
            providers::fakemail::get_emails(t, email)
        }
        Channel::Openinbox => {
            let t = token.ok_or("token is required for openinbox")?;
            providers::openinbox::get_emails(t, email)
        }
        Channel::Inboxes => providers::inboxes::get_emails(email),
        Channel::Uncorreotemporal => {
            let t = token.ok_or("token is required for uncorreotemporal")?;
            providers::uncorreotemporal::get_emails(t, email)
        }
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
        Channel::GuerrillamailCom => {
            let t = token.ok_or("token is required for guerrillamail-com")?;
            providers::guerrillamail_mirrors::guerrillamail_com::get_emails(t, email)
        }
        Channel::Maildrop => {
            let t = token.ok_or("token is required for maildrop")?;
            providers::maildrop::get_emails(t, email)
        }
        Channel::SmailPw => {
            let t = token.ok_or("token is required for smail-pw")?;
            providers::smail_pw::get_emails(t, email)
        }
        Channel::Vip215 => {
            let t = token.ok_or("token is required for vip-215")?;
            providers::vip_215::get_emails(t, email)
        }
        Channel::FakeLegal => providers::fake_legal::get_emails(email),
        Channel::Moakt => {
            let t = token.ok_or("token is required for moakt")?;
            providers::moakt::get_emails(t, email)
        }
        Channel::Email10min => {
            let t = token.ok_or("token is required for email10min")?;
            providers::email10min::get_emails(t, email)
        }
        Channel::MjjCm => providers::socketio_mail::mjj_cm::get_emails(email),
        Channel::LinshiCo => providers::socketio_mail::linshi_co::get_emails(email),
        Channel::Harakirimail => providers::harakirimail::get_emails(email),
        Channel::TempmailPlus => providers::tempmail_plus::get_emails(email),
        Channel::MailGw => {
            let t = token.ok_or("token is required for mail-gw")?;
            providers::mail_gw::get_emails(t, email)
        }
        Channel::TempmailLolV2 => {
            let t = token.ok_or("token is required for tempmail-lol-v2")?;
            providers::tempmail_lol_v2::get_emails(t, email)
        }
        Channel::Sharklasers => {
            let t = token.ok_or("token is required for sharklasers")?;
            providers::guerrillamail_mirrors::sharklasers::get_emails(t, email)
        }
        Channel::SharklasersCom => {
            let t = token.ok_or("token is required for sharklasers-com")?;
            providers::guerrillamail_mirrors::sharklasers_com::get_emails(t, email)
        }
        Channel::GrrLa => {
            let t = token.ok_or("token is required for grr-la")?;
            providers::guerrillamail_mirrors::grr_la::get_emails(t, email)
        }
        Channel::GrrLaCom => {
            let t = token.ok_or("token is required for grr-la-com")?;
            providers::guerrillamail_mirrors::grr_la_com::get_emails(t, email)
        }
        Channel::GuerrillamailInfo => {
            let t = token.ok_or("token is required for guerrillamail-info")?;
            providers::guerrillamail_mirrors::guerrillamail_info::get_emails(t, email)
        }
        Channel::Spam4me => {
            let t = token.ok_or("token is required for spam4me")?;
            providers::guerrillamail_mirrors::spam4me::get_emails(t, email)
        }
        Channel::GuerrillamailNet => {
            let t = token.ok_or("token is required for guerrillamail-net")?;
            providers::guerrillamail_mirrors::guerrillamail_net::get_emails(t, email)
        }
        Channel::GuerrillamailOrg => {
            let t = token.ok_or("token is required for guerrillamail-org")?;
            providers::guerrillamail_mirrors::guerrillamail_org::get_emails(t, email)
        }
        Channel::Guerrillamailblock => {
            let t = token.ok_or("token is required for guerrillamailblock")?;
            providers::guerrillamail_mirrors::guerrillamailblock::get_emails(t, email)
        }
        Channel::GuerrillamailComWww => {
            let t = token.ok_or("token is required for guerrillamail-com-www")?;
            providers::guerrillamail_mirrors::guerrillamail_com_www::get_emails(t, email)
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
