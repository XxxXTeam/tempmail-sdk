/*!
 * 渠道注册表
 *
 * 单一注册中心：每个渠道只在 `build_registry` 里追加一处 `ChannelSpec`，
 * 渠道有序列表（`all_channels`）、信息映射（`get_channel_info`）、
 * 创建/收信分发逻辑全部由该表自动派生，无需再手动同步多处平行结构。
 *
 * 注册顺序即枚举顺序（硬约束，五端一致，与仓库根 `.baseline_channels.txt` 对齐）。
 * 闭包体逐一对应重构前 `client.rs` 中三处 match 分支，调用语义完全不变。
 */

use crate::providers;
use crate::types::*;
use std::collections::HashMap;
use std::sync::OnceLock;

/// 单个渠道的注册规格。
/// 用不捕获环境的 `Box<dyn Fn>` 闭包承载创建/收信实现（对应 Go 端的 Generate/GetEmails 闭包），
/// 因此天然 `Send + Sync`，可安全缓存进全局 `OnceLock`。
#[allow(clippy::type_complexity)]
pub struct ChannelSpec {
    /// 渠道标识
    pub channel: Channel,
    /// 渠道显示名称
    pub name: &'static str,
    /// 对应的临时邮箱服务网站
    pub website: &'static str,
    /// 创建邮箱实现（对应原 `generate_email_once` 中该渠道分支体）
    pub generate: Box<dyn Fn(u32, Option<&str>) -> Result<EmailInfo, String> + Send + Sync>,
    /// 获取邮件实现（对应原 `get_emails_once` 中该渠道分支体）
    pub get_emails: Box<dyn Fn(&str, Option<&str>) -> Result<Vec<Email>, String> + Send + Sync>,
}

/// 覆盖固定域名/镜像域名类渠道：沿用底层 provider 结果，仅改写归属渠道标识。
/// 对应重构前 `client.rs` 的私有 `with_channel`。
fn with_channel(mut info: EmailInfo, channel: Channel) -> EmailInfo {
    info.channel = channel;
    info
}

/// 按顺序构造全部渠道注册规格。
/// 新增渠道只需在文件末尾追加一处 `reg.push(ChannelSpec{...})`。
#[allow(unused_variables)]
fn build_registry() -> Vec<ChannelSpec> {
    let mut reg: Vec<ChannelSpec> = Vec::new();

    reg.push(ChannelSpec {
        channel: Channel::Tempmail,
        name: "TempMail",
        website: "tempmail.ing",
        generate: Box::new(|duration, domain| providers::tempmail::generate_email(duration)),
        get_emails: Box::new(|email, token| providers::tempmail::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::TempmailCn,
        name: "TempMail CN",
        website: "tempmail.cn",
        generate: Box::new(|duration, domain| providers::tempmail_cn::generate_email(domain)),
        get_emails: Box::new(|email, token| providers::tempmail_cn::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::TaEasy,
        name: "TA Easy",
        website: "ta-easy.com",
        generate: Box::new(|duration, domain| providers::ta_easy::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for ta-easy")?;
            providers::ta_easy::get_emails(email, t)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::TenminuteOne,
        name: "10 Minute Email",
        website: "10minutemail.one",
        generate: Box::new(|duration, domain| providers::tenminute_one::generate_email(domain)),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for 10minute-one")?;
            providers::tenminute_one::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::XghffCom,
        name: "xghff.com",
        website: "10minutemail.one",
        generate: Box::new(|duration, domain| {
            providers::tenminute_one::generate_email(Some("xghff.com"))
                .map(|info| with_channel(info, Channel::XghffCom))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for 10minute-one")?;
            providers::tenminute_one::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::OqqajCom,
        name: "oqqaj.com",
        website: "10minutemail.one",
        generate: Box::new(|duration, domain| {
            providers::tenminute_one::generate_email(Some("oqqaj.com"))
                .map(|info| with_channel(info, Channel::OqqajCom))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for 10minute-one")?;
            providers::tenminute_one::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::PsovvCom,
        name: "psovv.com",
        website: "10minutemail.one",
        generate: Box::new(|duration, domain| {
            providers::tenminute_one::generate_email(Some("psovv.com"))
                .map(|info| with_channel(info, Channel::PsovvCom))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for 10minute-one")?;
            providers::tenminute_one::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::DbwotCom,
        name: "dbwot.com",
        website: "10minutemail.one",
        generate: Box::new(|duration, domain| {
            providers::tenminute_one::generate_email(Some("dbwot.com"))
                .map(|info| with_channel(info, Channel::DbwotCom))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for 10minute-one")?;
            providers::tenminute_one::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::YgwprCom,
        name: "ygwpr.com",
        website: "10minutemail.one",
        generate: Box::new(|duration, domain| {
            providers::tenminute_one::generate_email(Some("ygwpr.com"))
                .map(|info| with_channel(info, Channel::YgwprCom))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for 10minute-one")?;
            providers::tenminute_one::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::ImxweCom,
        name: "imxwe.com",
        website: "10minutemail.one",
        generate: Box::new(|duration, domain| {
            providers::tenminute_one::generate_email(Some("imxwe.com"))
                .map(|info| with_channel(info, Channel::ImxweCom))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for 10minute-one")?;
            providers::tenminute_one::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Linshiyou,
        name: "临时邮",
        website: "linshiyou.com",
        generate: Box::new(|duration, domain| providers::linshiyou::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for linshiyou")?;
            providers::linshiyou::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Mffac,
        name: "MFFAC",
        website: "mffac.com",
        generate: Box::new(|duration, domain| providers::mffac::generate_email()),
        get_emails: Box::new(providers::mffac::get_emails),
    });

    reg.push(ChannelSpec {
        channel: Channel::TempmailLol,
        name: "TempMail LOL",
        website: "tempmail.lol",
        generate: Box::new(|duration, domain| providers::tempmail_lol::generate_email(domain)),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for tempmail-lol")?;
            providers::tempmail_lol::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::ChatgptOrgUk,
        name: "ChatGPT Mail",
        website: "mail.chatgpt.org.uk",
        generate: Box::new(|duration, domain| providers::chatgpt_org_uk::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for chatgpt-org-uk")?;
            providers::chatgpt_org_uk::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::TempMailIo,
        name: "Temp-Mail.io",
        website: "temp-mail.io",
        generate: Box::new(|duration, domain| providers::temp_mail_io::generate_email()),
        get_emails: Box::new(|email, token| providers::temp_mail_io::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::MailCx,
        name: "Mail.cx",
        website: "mail.cx",
        generate: Box::new(|duration, domain| providers::mail_cx::generate_email(domain)),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for mail-cx")?;
            providers::mail_cx::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::DdkerCom,
        name: "ddker.com",
        website: "mail.cx",
        generate: Box::new(|duration, domain| {
            providers::mail_cx::generate_email(Some("ddker.com"))
                .map(|info| with_channel(info, Channel::DdkerCom))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for mail-cx")?;
            providers::mail_cx::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Catchmail,
        name: "Catchmail",
        website: "catchmail.io",
        generate: Box::new(|duration, domain| providers::catchmail::generate_email(domain)),
        get_emails: Box::new(|email, token| providers::catchmail::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::CatchmailMailistry,
        name: "Catchmail Mailistry",
        website: "mailistry.com",
        generate: Box::new(|duration, domain| {
            providers::catchmail::generate_email(Some("mailistry.com"))
                .map(|info| with_channel(info, Channel::CatchmailMailistry))
        }),
        get_emails: Box::new(|email, token| providers::catchmail::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::CatchmailZeppost,
        name: "Catchmail Zeppost",
        website: "zeppost.com",
        generate: Box::new(|duration, domain| {
            providers::catchmail::generate_email(Some("zeppost.com"))
                .map(|info| with_channel(info, Channel::CatchmailZeppost))
        }),
        get_emails: Box::new(|email, token| providers::catchmail::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Mailforspam,
        name: "MailForSpam",
        website: "mailforspam.com",
        generate: Box::new(|duration, domain| providers::mailforspam::generate_email(domain)),
        get_emails: Box::new(|email, token| providers::mailforspam::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::MailforspamTempmailIo,
        name: "MailForSpam TempMail.io",
        website: "tempmail.io",
        generate: Box::new(|duration, domain| {
            {
                providers::mailforspam::generate_email(Some("tempmail.io"))
                    .map(|info| with_channel(info, Channel::MailforspamTempmailIo))
            }
        }),
        get_emails: Box::new(|email, token| providers::mailforspam::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::MailforspamDisposable,
        name: "MailForSpam Disposable",
        website: "disposable.email",
        generate: Box::new(|duration, domain| {
            {
                providers::mailforspam::generate_email(Some("disposable.email"))
                    .map(|info| with_channel(info, Channel::MailforspamDisposable))
            }
        }),
        get_emails: Box::new(|email, token| providers::mailforspam::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Tempmailc,
        name: "TempMailC",
        website: "tempmailc.com",
        generate: Box::new(|duration, domain| providers::tempmailc::generate_email()),
        get_emails: Box::new(|email, token| providers::tempmailc::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Mailnesia,
        name: "Mailnesia",
        website: "mailnesia.com",
        generate: Box::new(|duration, domain| providers::mailnesia::generate_email()),
        get_emails: Box::new(|email, token| providers::mailnesia::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Throwawaymail,
        name: "ThrowawayMail",
        website: "throwawaymail.app",
        generate: Box::new(|duration, domain| providers::throwawaymail::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for throwawaymail")?;
            providers::throwawaymail::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::TempmailFish,
        name: "TempMail Fish",
        website: "tempmail.fish",
        generate: Box::new(|duration, domain| providers::tempmail_fish::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for tempmail-fish")?;
            providers::tempmail_fish::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::NeighboursSh,
        name: "Neighbours",
        website: "neighbours.sh",
        generate: Box::new(|duration, domain| providers::neighbours_sh::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for neighbours-sh")?;
            providers::neighbours_sh::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::ShittyEmail,
        name: "shitty.email",
        website: "shitty.email",
        generate: Box::new(|duration, domain| providers::shitty_email::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for shitty-email")?;
            providers::shitty_email::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Tempmailpro,
        name: "TempMail Pro",
        website: "tempmailpro.us",
        generate: Box::new(|duration, domain| providers::tempmailpro::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for tempmailpro")?;
            providers::tempmailpro::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::DevmailUk,
        name: "DevMail UK",
        website: "devmail.uk",
        generate: Box::new(|duration, domain| providers::devmail_uk::generate_email()),
        get_emails: Box::new(|email, token| providers::devmail_uk::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Inboxkitten,
        name: "InboxKitten",
        website: "inboxkitten.com",
        generate: Box::new(|duration, domain| providers::inboxkitten::generate_email()),
        get_emails: Box::new(|email, token| providers::inboxkitten::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::CleanTempMail,
        name: "CleanTempMail",
        website: "cleantempmail.com",
        generate: Box::new(|duration, domain| providers::cleantempmail::generate_email()),
        get_emails: Box::new(|email, token| providers::cleantempmail::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Getnada,
        name: "GetNada",
        website: "getnada.net",
        generate: Box::new(|duration, domain| providers::getnada::generate_email(domain)),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::OneVpnNet,
        name: "1vpn.net",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            providers::getnada::generate_email(Some("1vpn.net"))
                .map(|info| with_channel(info, Channel::OneVpnNet))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::AbematvCom,
        name: "abematv.com",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            providers::getnada::generate_email(Some("abematv.com"))
                .map(|info| with_channel(info, Channel::AbematvCom))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::AbematvNet,
        name: "abematv.net",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            providers::getnada::generate_email(Some("abematv.net"))
                .map(|info| with_channel(info, Channel::AbematvNet))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::AbematvOrg,
        name: "abematv.org",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            providers::getnada::generate_email(Some("abematv.org"))
                .map(|info| with_channel(info, Channel::AbematvOrg))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::AcehCc,
        name: "aceh.cc",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            providers::getnada::generate_email(Some("aceh.cc"))
                .map(|info| with_channel(info, Channel::AcehCc))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::BangkabelitungNet,
        name: "bangkabelitung.net",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            {
                providers::getnada::generate_email(Some("bangkabelitung.net"))
                    .map(|info| with_channel(info, Channel::BangkabelitungNet))
            }
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::CctruyenCom,
        name: "cctruyen.com",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            providers::getnada::generate_email(Some("cctruyen.com"))
                .map(|info| with_channel(info, Channel::CctruyenCom))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::GetnadaCom,
        name: "getnada.com",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            providers::getnada::generate_email(Some("getnada.com"))
                .map(|info| with_channel(info, Channel::GetnadaCom))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::GetnadaEmail,
        name: "getnada.email",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            providers::getnada::generate_email(Some("getnada.email"))
                .map(|info| with_channel(info, Channel::GetnadaEmail))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::GetnadaNet,
        name: "getnada.net",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            providers::getnada::generate_email(Some("getnada.net"))
                .map(|info| with_channel(info, Channel::GetnadaNet))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::JawatengahNet,
        name: "jawatengah.net",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            providers::getnada::generate_email(Some("jawatengah.net"))
                .map(|info| with_channel(info, Channel::JawatengahNet))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::JawatimurNet,
        name: "jawatimur.net",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            providers::getnada::generate_email(Some("jawatimur.net"))
                .map(|info| with_channel(info, Channel::JawatimurNet))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::KalimantanbaratNet,
        name: "kalimantanbarat.net",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            {
                providers::getnada::generate_email(Some("kalimantanbarat.net"))
                    .map(|info| with_channel(info, Channel::KalimantanbaratNet))
            }
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::KalimantanselatanNet,
        name: "kalimantanselatan.net",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            {
                providers::getnada::generate_email(Some("kalimantanselatan.net"))
                    .map(|info| with_channel(info, Channel::KalimantanselatanNet))
            }
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::KalimantantengahNet,
        name: "kalimantantengah.net",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            {
                providers::getnada::generate_email(Some("kalimantantengah.net"))
                    .map(|info| with_channel(info, Channel::KalimantantengahNet))
            }
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::KalimantantimurNet,
        name: "kalimantantimur.net",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            {
                providers::getnada::generate_email(Some("kalimantantimur.net"))
                    .map(|info| with_channel(info, Channel::KalimantantimurNet))
            }
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::KalimantanutaraNet,
        name: "kalimantanutara.net",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            {
                providers::getnada::generate_email(Some("kalimantanutara.net"))
                    .map(|info| with_channel(info, Channel::KalimantanutaraNet))
            }
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::KepulauanriauNet,
        name: "kepulauanriau.net",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            providers::getnada::generate_email(Some("kepulauanriau.net"))
                .map(|info| with_channel(info, Channel::KepulauanriauNet))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Luxury345Com,
        name: "luxury345.com",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            providers::getnada::generate_email(Some("luxury345.com"))
                .map(|info| with_channel(info, Channel::Luxury345Com))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::MalukuutaraNet,
        name: "malukuutara.net",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            providers::getnada::generate_email(Some("malukuutara.net"))
                .map(|info| with_channel(info, Channel::MalukuutaraNet))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::NusatenggarabaratNet,
        name: "nusatenggarabarat.net",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            {
                providers::getnada::generate_email(Some("nusatenggarabarat.net"))
                    .map(|info| with_channel(info, Channel::NusatenggarabaratNet))
            }
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::NusatenggaratimurNet,
        name: "nusatenggaratimur.net",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            {
                providers::getnada::generate_email(Some("nusatenggaratimur.net"))
                    .map(|info| with_channel(info, Channel::NusatenggaratimurNet))
            }
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::PapuabaratNet,
        name: "papuabarat.net",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            providers::getnada::generate_email(Some("papuabarat.net"))
                .map(|info| with_channel(info, Channel::PapuabaratNet))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::PapuabaratdayaNet,
        name: "papuabaratdaya.net",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            {
                providers::getnada::generate_email(Some("papuabaratdaya.net"))
                    .map(|info| with_channel(info, Channel::PapuabaratdayaNet))
            }
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::PapuaselatanNet,
        name: "papuaselatan.net",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            providers::getnada::generate_email(Some("papuaselatan.net"))
                .map(|info| with_channel(info, Channel::PapuaselatanNet))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::PeholCom,
        name: "pehol.com",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            providers::getnada::generate_email(Some("pehol.com"))
                .map(|info| with_channel(info, Channel::PeholCom))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::PtruyenCom,
        name: "ptruyen.com",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            providers::getnada::generate_email(Some("ptruyen.com"))
                .map(|info| with_channel(info, Channel::PtruyenCom))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::PulaubaliNet,
        name: "pulaubali.net",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            providers::getnada::generate_email(Some("pulaubali.net"))
                .map(|info| with_channel(info, Channel::PulaubaliNet))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::RiauNet,
        name: "riau.net",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            providers::getnada::generate_email(Some("riau.net"))
                .map(|info| with_channel(info, Channel::RiauNet))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::SeokeyOrg,
        name: "seokey.org",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            providers::getnada::generate_email(Some("seokey.org"))
                .map(|info| with_channel(info, Channel::SeokeyOrg))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::SulawesibaratNet,
        name: "sulawesibarat.net",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            providers::getnada::generate_email(Some("sulawesibarat.net"))
                .map(|info| with_channel(info, Channel::SulawesibaratNet))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::SulawesiselatanNet,
        name: "sulawesiselatan.net",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            {
                providers::getnada::generate_email(Some("sulawesiselatan.net"))
                    .map(|info| with_channel(info, Channel::SulawesiselatanNet))
            }
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::SulawesitengahNet,
        name: "sulawesitengah.net",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            {
                providers::getnada::generate_email(Some("sulawesitengah.net"))
                    .map(|info| with_channel(info, Channel::SulawesitengahNet))
            }
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::SulawesitenggaraNet,
        name: "sulawesitenggara.net",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            {
                providers::getnada::generate_email(Some("sulawesitenggara.net"))
                    .map(|info| with_channel(info, Channel::SulawesitenggaraNet))
            }
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::SumaterabaratNet,
        name: "sumaterabarat.net",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            providers::getnada::generate_email(Some("sumaterabarat.net"))
                .map(|info| with_channel(info, Channel::SumaterabaratNet))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::SumateraselatanNet,
        name: "sumateraselatan.net",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            {
                providers::getnada::generate_email(Some("sumateraselatan.net"))
                    .map(|info| with_channel(info, Channel::SumateraselatanNet))
            }
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::SumaterautaraNet,
        name: "sumaterautara.net",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            providers::getnada::generate_email(Some("sumaterautara.net"))
                .map(|info| with_channel(info, Channel::SumaterautaraNet))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::VillatogelCom,
        name: "villatogel.com",
        website: "getnada.net",
        generate: Box::new(|duration, domain| {
            providers::getnada::generate_email(Some("villatogel.com"))
                .map(|info| with_channel(info, Channel::VillatogelCom))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Mail123,
        name: "Mail123",
        website: "mail123.fr",
        generate: Box::new(|duration, domain| providers::mail123::generate_email()),
        get_emails: Box::new(|email, token| providers::mail123::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Mail10s,
        name: "Mail10s",
        website: "mail10s.com",
        generate: Box::new(|duration, domain| providers::mail10s::generate_email()),
        get_emails: Box::new(|email, token| providers::mail10s::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Webmailtemp,
        name: "WebMailTemp",
        website: "webmailtemp.com",
        generate: Box::new(|duration, domain| providers::webmailtemp::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for webmailtemp")?;
            providers::webmailtemp::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Tempfastmail,
        name: "TempFastMail",
        website: "tempfastmail.com",
        generate: Box::new(|duration, domain| providers::tempfastmail::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for tempfastmail")?;
            providers::tempfastmail::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::OneSecMail,
        name: "1SecMail",
        website: "1sec-mail.com",
        generate: Box::new(|duration, domain| providers::one_sec_mail::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for 1sec-mail")?;
            providers::one_sec_mail::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Fakemail,
        name: "FakeMail",
        website: "fakemail.net",
        generate: Box::new(|duration, domain| providers::fakemail::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for fakemail")?;
            providers::fakemail::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Openinbox,
        name: "OpenInbox",
        website: "openinbox.io",
        generate: Box::new(|duration, domain| providers::openinbox::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for openinbox")?;
            providers::openinbox::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Inboxes,
        name: "Inboxes",
        website: "inboxes.com",
        generate: Box::new(|duration, domain| providers::inboxes::generate_email(domain)),
        get_emails: Box::new(|email, token| providers::inboxes::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Uncorreotemporal,
        name: "UnCorreoTemporal",
        website: "uncorreotemporal.com",
        generate: Box::new(|duration, domain| providers::uncorreotemporal::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for uncorreotemporal")?;
            providers::uncorreotemporal::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Awamail,
        name: "AwaMail",
        website: "awamail.com",
        generate: Box::new(|duration, domain| providers::awamail::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for awamail")?;
            providers::awamail::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::MailTm,
        name: "Mail.tm",
        website: "mail.tm",
        generate: Box::new(|duration, domain| providers::mail_tm::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for mail-tm")?;
            providers::mail_tm::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::WebLibraryNet,
        name: "web-library.net",
        website: "mail.tm",
        generate: Box::new(|duration, domain| {
            providers::mail_tm::generate_email()
                .map(|info| with_channel(info, Channel::WebLibraryNet))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for mail-tm")?;
            providers::mail_tm::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Dropmail,
        name: "DropMail",
        website: "dropmail.me",
        generate: Box::new(|duration, domain| providers::dropmail::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for dropmail")?;
            providers::dropmail::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::GuerrillaMail,
        name: "Guerrilla Mail",
        website: "guerrillamail.com",
        generate: Box::new(|duration, domain| providers::guerrillamail::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for guerrillamail")?;
            providers::guerrillamail::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::GuerrillamailCom,
        name: "GuerrillaMail Root",
        website: "guerrillamail.com",
        generate: Box::new(|duration, domain| {
            {
                providers::guerrillamail_mirrors::guerrillamail_com::generate_email()
            }
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for guerrillamail-com")?;
            providers::guerrillamail_mirrors::guerrillamail_com::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Maildrop,
        name: "Maildrop",
        website: "maildrop.cx",
        generate: Box::new(|duration, domain| providers::maildrop::generate_email(domain)),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for maildrop")?;
            providers::maildrop::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::SmailPw,
        name: "Smail.pw",
        website: "smail.pw",
        generate: Box::new(|duration, domain| providers::smail_pw::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for smail-pw")?;
            providers::smail_pw::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Vip215,
        name: "VIP 215",
        website: "vip.215.im",
        generate: Box::new(|duration, domain| providers::vip_215::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for vip-215")?;
            providers::vip_215::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::FakeLegal,
        name: "Fake Legal",
        website: "fake.legal",
        generate: Box::new(|duration, domain| providers::fake_legal::generate_email(domain)),
        get_emails: Box::new(|email, token| providers::fake_legal::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::ImguiDe,
        name: "imgui.de",
        website: "fake.legal",
        generate: Box::new(|duration, domain| {
            providers::fake_legal::generate_email(Some("imgui.de"))
                .map(|info| with_channel(info, Channel::ImguiDe))
        }),
        get_emails: Box::new(|email, token| providers::fake_legal::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::PulsewebmenuDe,
        name: "pulsewebmenu.de",
        website: "fake.legal",
        generate: Box::new(|duration, domain| {
            providers::fake_legal::generate_email(Some("pulsewebmenu.de"))
                .map(|info| with_channel(info, Channel::PulsewebmenuDe))
        }),
        get_emails: Box::new(|email, token| providers::fake_legal::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Moakt,
        name: "Moakt",
        website: "moakt.com",
        generate: Box::new(|duration, domain| providers::moakt::generate_email(domain)),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for moakt")?;
            providers::moakt::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::DrmailIn,
        name: "drmail.in",
        website: "moakt.com",
        generate: Box::new(|duration, domain| {
            providers::moakt::generate_email(Some("drmail.in"))
                .map(|info| with_channel(info, Channel::DrmailIn))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for moakt")?;
            providers::moakt::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::TemlNet,
        name: "teml.net",
        website: "moakt.com",
        generate: Box::new(|duration, domain| {
            providers::moakt::generate_email(Some("teml.net"))
                .map(|info| with_channel(info, Channel::TemlNet))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for moakt")?;
            providers::moakt::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::TmpemlCom,
        name: "tmpeml.com",
        website: "moakt.com",
        generate: Box::new(|duration, domain| {
            providers::moakt::generate_email(Some("tmpeml.com"))
                .map(|info| with_channel(info, Channel::TmpemlCom))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for moakt")?;
            providers::moakt::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::TmpboxNet,
        name: "tmpbox.net",
        website: "moakt.com",
        generate: Box::new(|duration, domain| {
            providers::moakt::generate_email(Some("tmpbox.net"))
                .map(|info| with_channel(info, Channel::TmpboxNet))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for moakt")?;
            providers::moakt::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::MoaktCc,
        name: "moakt.cc",
        website: "moakt.com",
        generate: Box::new(|duration, domain| {
            providers::moakt::generate_email(Some("moakt.cc"))
                .map(|info| with_channel(info, Channel::MoaktCc))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for moakt")?;
            providers::moakt::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::DisboxNet,
        name: "disbox.net",
        website: "moakt.com",
        generate: Box::new(|duration, domain| {
            providers::moakt::generate_email(Some("disbox.net"))
                .map(|info| with_channel(info, Channel::DisboxNet))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for moakt")?;
            providers::moakt::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::TmpmailOrg,
        name: "tmpmail.org",
        website: "moakt.com",
        generate: Box::new(|duration, domain| {
            providers::moakt::generate_email(Some("tmpmail.org"))
                .map(|info| with_channel(info, Channel::TmpmailOrg))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for moakt")?;
            providers::moakt::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::TmpmailNet,
        name: "tmpmail.net",
        website: "moakt.com",
        generate: Box::new(|duration, domain| {
            providers::moakt::generate_email(Some("tmpmail.net"))
                .map(|info| with_channel(info, Channel::TmpmailNet))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for moakt")?;
            providers::moakt::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::TmailsNet,
        name: "tmails.net",
        website: "moakt.com",
        generate: Box::new(|duration, domain| {
            providers::moakt::generate_email(Some("tmails.net"))
                .map(|info| with_channel(info, Channel::TmailsNet))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for moakt")?;
            providers::moakt::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::DisboxOrg,
        name: "disbox.org",
        website: "moakt.com",
        generate: Box::new(|duration, domain| {
            providers::moakt::generate_email(Some("disbox.org"))
                .map(|info| with_channel(info, Channel::DisboxOrg))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for moakt")?;
            providers::moakt::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::MoaktCo,
        name: "moakt.co",
        website: "moakt.com",
        generate: Box::new(|duration, domain| {
            providers::moakt::generate_email(Some("moakt.co"))
                .map(|info| with_channel(info, Channel::MoaktCo))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for moakt")?;
            providers::moakt::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::MoaktWs,
        name: "moakt.ws",
        website: "moakt.com",
        generate: Box::new(|duration, domain| {
            providers::moakt::generate_email(Some("moakt.ws"))
                .map(|info| with_channel(info, Channel::MoaktWs))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for moakt")?;
            providers::moakt::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::TmailWs,
        name: "tmail.ws",
        website: "moakt.com",
        generate: Box::new(|duration, domain| {
            providers::moakt::generate_email(Some("tmail.ws"))
                .map(|info| with_channel(info, Channel::TmailWs))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for moakt")?;
            providers::moakt::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::BareedWs,
        name: "bareed.ws",
        website: "moakt.com",
        generate: Box::new(|duration, domain| {
            providers::moakt::generate_email(Some("bareed.ws"))
                .map(|info| with_channel(info, Channel::BareedWs))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for moakt")?;
            providers::moakt::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Email10min,
        name: "Email10Min",
        website: "email10min.com",
        generate: Box::new(|duration, domain| providers::email10min::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for email10min")?;
            providers::email10min::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::MjjCm,
        name: "MJJ Mail",
        website: "mjj.cm",
        generate: Box::new(|duration, domain| providers::socketio_mail::mjj_cm::generate_email()),
        get_emails: Box::new(|email, token| providers::socketio_mail::mjj_cm::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::LinshiCo,
        name: "临时Co",
        website: "linshi.co",
        generate: Box::new(|duration, domain| {
            providers::socketio_mail::linshi_co::generate_email()
        }),
        get_emails: Box::new(|email, token| providers::socketio_mail::linshi_co::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Harakirimail,
        name: "HarakiriMail",
        website: "harakirimail.com",
        generate: Box::new(|duration, domain| providers::harakirimail::generate_email()),
        get_emails: Box::new(|email, token| providers::harakirimail::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::JqkjqkXyz,
        name: "jqkjqk.xyz",
        website: "mail.zhujump.com",
        generate: Box::new(|duration, domain| {
            providers::zhujump::generate_email("jqkjqk.xyz", Channel::JqkjqkXyz)
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for moemail")?;
            providers::zhujump::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::LyhleviCom,
        name: "LyhLevi MoeMail",
        website: "lyhlevi.com",
        generate: Box::new(|duration, domain| providers::m2u::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for moemail")?;
            providers::zhujump::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::TempmailPlus,
        name: "TempMail Plus",
        website: "tempmail.plus",
        generate: Box::new(|duration, domain| {
            {
                providers::tempmail_plus::generate_email(None, Channel::TempmailPlus)
            }
        }),
        get_emails: Box::new(|email, token| providers::tempmail_plus::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::FexpostCom,
        name: "fexpost.com",
        website: "tempmail.plus",
        generate: Box::new(|duration, domain| {
            {
                providers::tempmail_plus::generate_email(Some("fexpost.com"), Channel::FexpostCom)
            }
        }),
        get_emails: Box::new(|email, token| providers::tempmail_plus::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::FexboxOrg,
        name: "fexbox.org",
        website: "tempmail.plus",
        generate: Box::new(|duration, domain| {
            {
                providers::tempmail_plus::generate_email(Some("fexbox.org"), Channel::FexboxOrg)
            }
        }),
        get_emails: Box::new(|email, token| providers::tempmail_plus::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::MailboxInUa,
        name: "mailbox.in.ua",
        website: "tempmail.plus",
        generate: Box::new(|duration, domain| {
            {
                providers::tempmail_plus::generate_email(
                    Some("mailbox.in.ua"),
                    Channel::MailboxInUa,
                )
            }
        }),
        get_emails: Box::new(|email, token| providers::tempmail_plus::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::RoverInfo,
        name: "rover.info",
        website: "tempmail.plus",
        generate: Box::new(|duration, domain| {
            {
                providers::tempmail_plus::generate_email(Some("rover.info"), Channel::RoverInfo)
            }
        }),
        get_emails: Box::new(|email, token| providers::tempmail_plus::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::ChitthiIn,
        name: "chitthi.in",
        website: "tempmail.plus",
        generate: Box::new(|duration, domain| {
            {
                providers::tempmail_plus::generate_email(Some("chitthi.in"), Channel::ChitthiIn)
            }
        }),
        get_emails: Box::new(|email, token| providers::tempmail_plus::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::FextempCom,
        name: "fextemp.com",
        website: "tempmail.plus",
        generate: Box::new(|duration, domain| {
            {
                providers::tempmail_plus::generate_email(Some("fextemp.com"), Channel::FextempCom)
            }
        }),
        get_emails: Box::new(|email, token| providers::tempmail_plus::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::AnyPink,
        name: "any.pink",
        website: "tempmail.plus",
        generate: Box::new(|duration, domain| {
            {
                providers::tempmail_plus::generate_email(Some("any.pink"), Channel::AnyPink)
            }
        }),
        get_emails: Box::new(|email, token| providers::tempmail_plus::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::MerepostCom,
        name: "merepost.com",
        website: "tempmail.plus",
        generate: Box::new(|duration, domain| {
            {
                providers::tempmail_plus::generate_email(Some("merepost.com"), Channel::MerepostCom)
            }
        }),
        get_emails: Box::new(|email, token| providers::tempmail_plus::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::TempmailLolV2,
        name: "TempMail LOL V2",
        website: "tempmail.lol",
        generate: Box::new(|duration, domain| providers::tempmail_lol_v2::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for tempmail-lol-v2")?;
            providers::tempmail_lol_v2::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Tempgbox,
        name: "TempGBox",
        website: "tempgbox.net",
        generate: Box::new(|duration, domain| providers::tempgbox::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for tempgbox")?;
            providers::tempgbox::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Emailnator,
        name: "Emailnator",
        website: "emailnator.com",
        generate: Box::new(|duration, domain| providers::emailnator::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for emailnator")?;
            providers::emailnator::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Temporam,
        name: "Temporam",
        website: "temporam.com",
        generate: Box::new(|duration, domain| providers::temporam::generate_email(domain)),
        get_emails: Box::new(|email, token| providers::temporam::get_emails(token, email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Neighbours,
        name: "Neighbours",
        website: "neighbours.sh",
        generate: Box::new(|duration, domain| providers::neighbours::generate_email(domain)),
        get_emails: Box::new(|email, token| providers::neighbours::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Sharklasers,
        name: "SharkLasers",
        website: "sharklasers.com",
        generate: Box::new(|duration, domain| {
            providers::guerrillamail_mirrors::sharklasers::generate_email()
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for sharklasers")?;
            providers::guerrillamail_mirrors::sharklasers::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::SharklasersCom,
        name: "SharkLasers Root",
        website: "sharklasers.com",
        generate: Box::new(|duration, domain| {
            {
                providers::guerrillamail_mirrors::sharklasers_com::generate_email()
            }
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for sharklasers-com")?;
            providers::guerrillamail_mirrors::sharklasers_com::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::GrrLa,
        name: "Grr.la",
        website: "grr.la",
        generate: Box::new(|duration, domain| {
            providers::guerrillamail_mirrors::grr_la::generate_email()
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for grr-la")?;
            providers::guerrillamail_mirrors::grr_la::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::GrrLaCom,
        name: "Grr.la Root",
        website: "grr.la",
        generate: Box::new(|duration, domain| {
            providers::guerrillamail_mirrors::grr_la_com::generate_email()
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for grr-la-com")?;
            providers::guerrillamail_mirrors::grr_la_com::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::GuerrillamailInfo,
        name: "GuerrillaMail Info",
        website: "guerrillamail.info",
        generate: Box::new(|duration, domain| {
            {
                providers::guerrillamail_mirrors::guerrillamail_info::generate_email()
            }
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for guerrillamail-info")?;
            providers::guerrillamail_mirrors::guerrillamail_info::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Spam4me,
        name: "Spam4.me",
        website: "spam4.me",
        generate: Box::new(|duration, domain| {
            providers::guerrillamail_mirrors::spam4me::generate_email()
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for spam4me")?;
            providers::guerrillamail_mirrors::spam4me::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::GuerrillamailNet,
        name: "GuerrillaMail Net",
        website: "guerrillamail.net",
        generate: Box::new(|duration, domain| {
            {
                providers::guerrillamail_mirrors::guerrillamail_net::generate_email()
            }
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for guerrillamail-net")?;
            providers::guerrillamail_mirrors::guerrillamail_net::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::GuerrillamailOrg,
        name: "GuerrillaMail Org",
        website: "guerrillamail.org",
        generate: Box::new(|duration, domain| {
            {
                providers::guerrillamail_mirrors::guerrillamail_org::generate_email()
            }
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for guerrillamail-org")?;
            providers::guerrillamail_mirrors::guerrillamail_org::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Guerrillamailblock,
        name: "GuerrillaMailBlock",
        website: "guerrillamailblock.com",
        generate: Box::new(|duration, domain| {
            {
                providers::guerrillamail_mirrors::guerrillamailblock::generate_email()
            }
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for guerrillamailblock")?;
            providers::guerrillamail_mirrors::guerrillamailblock::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::GuerrillamailComWww,
        name: "GuerrillaMail WWW",
        website: "guerrillamail.com",
        generate: Box::new(|duration, domain| {
            {
                providers::guerrillamail_mirrors::guerrillamail_com_www::generate_email()
            }
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for guerrillamail-com-www")?;
            providers::guerrillamail_mirrors::guerrillamail_com_www::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::M2u,
        name: "MailToYou",
        website: "m2u.io",
        generate: Box::new(|duration, domain| providers::m2u::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for m2u")?;
            providers::m2u::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::TempyEmail,
        name: "Tempy Email",
        website: "tempy.email",
        generate: Box::new(|duration, domain| providers::tempy_email::generate_email(domain)),
        get_emails: Box::new(|email, token| providers::tempy_email::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Fmail,
        name: "FMail",
        website: "fmail.men",
        generate: Box::new(|duration, domain| providers::fmail::generate_email(domain)),
        get_emails: Box::new(|email, token| providers::fmail::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Ockito,
        name: "Ockito",
        website: "ockito.com",
        generate: Box::new(|duration, domain| providers::ockito::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for ockito")?;
            providers::ockito::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Anonbox,
        name: "Anonbox",
        website: "anonbox.net",
        generate: Box::new(|duration, domain| providers::anonbox::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for anonbox")?;
            providers::anonbox::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Duckmail,
        name: "DuckMail",
        website: "duckmail.sbs",
        generate: Box::new(|duration, domain| providers::duckmail::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for duckmail")?;
            providers::duckmail::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Mailinator,
        name: "Mailinator",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::mailinator::generate_email()),
        get_emails: Box::new(|email, token| providers::mailinator::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Tempmail365,
        name: "Tempmail365",
        website: "https://tempmail365.cn",
        generate: Box::new(|duration, domain| providers::tempmail365::generate_email(domain)),
        get_emails: Box::new(|email, token| providers::tempmail365::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Tempinbox,
        name: "TempInbox",
        website: "https://www.tempinbox.xyz",
        generate: Box::new(|duration, domain| providers::tempinbox::generate_email(domain)),
        get_emails: Box::new(|email, token| providers::tempinbox::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Byom,
        name: "Byom",
        website: "byom.de",
        generate: Box::new(|duration, domain| providers::byom::generate_email()),
        get_emails: Box::new(|email, token| providers::byom::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Anonymmail,
        name: "AnonymMail",
        website: "anonymmail.net",
        generate: Box::new(|duration, domain| providers::anonymmail::generate_email()),
        get_emails: Box::new(|email, token| providers::anonymmail::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Eyepaste,
        name: "EyePaste",
        website: "eyepaste.com",
        generate: Box::new(|duration, domain| providers::eyepaste::generate_email()),
        get_emails: Box::new(|email, token| providers::eyepaste::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::MailSunls,
        name: "Mail Sunls",
        website: "mail.sunls.de",
        generate: Box::new(|duration, domain| providers::mail_sunls::generate_email()),
        get_emails: Box::new(|email, token| providers::mail_sunls::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Expressinboxhub,
        name: "ExpressInboxHub",
        website: "expressinboxhub.com",
        generate: Box::new(|duration, domain| providers::expressinboxhub::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for expressinboxhub")?;
            providers::expressinboxhub::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Lroid,
        name: "Lroid",
        website: "lroid.com",
        generate: Box::new(|duration, domain| providers::lroid::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for lroid")?;
            providers::lroid::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Haribu,
        name: "Haribu",
        website: "haribu.net",
        generate: Box::new(|duration, domain| providers::haribu::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for haribu")?;
            providers::haribu::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Rootsh,
        name: "Rootsh(BccTo)",
        website: "rootsh.com",
        generate: Box::new(|duration, domain| providers::rootsh::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for rootsh")?;
            providers::rootsh::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::FakeEmailSite,
        name: "FakeEmailSite",
        website: "fake-email.site",
        generate: Box::new(|duration, domain| providers::fake_email_site::generate_email()),
        get_emails: Box::new(|email, token| providers::fake_email_site::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Mohmal,
        name: "Mohmal",
        website: "mohmal.com",
        generate: Box::new(|duration, domain| providers::mohmal::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for mohmal")?;
            providers::mohmal::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Mailgolem,
        name: "MailGolem",
        website: "mailgolem.com",
        generate: Box::new(|duration, domain| providers::mailgolem::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for mailgolem")?;
            providers::mailgolem::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::BestTempMail,
        name: "BestTempMail",
        website: "best-temp-mail.com",
        generate: Box::new(|duration, domain| providers::best_temp_mail::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for best-temp-mail")?;
            providers::best_temp_mail::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::DisposablemailApp,
        name: "DisposableMail",
        website: "disposablemail.app",
        generate: Box::new(|duration, domain| providers::disposablemail_app::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for disposablemail-app")?;
            providers::disposablemail_app::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::MailtempCc,
        name: "MailTemp.cc",
        website: "mailtemp.cc",
        generate: Box::new(|duration, domain| providers::mailtemp_cc::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for mailtemp-cc")?;
            providers::mailtemp_cc::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Minuteinbox,
        name: "MinuteInbox",
        website: "minuteinbox.com",
        generate: Box::new(|duration, domain| providers::minuteinbox::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for minuteinbox")?;
            providers::minuteinbox::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Mailcatch,
        name: "MailCatch",
        website: "mailcatch.com",
        generate: Box::new(|duration, domain| {
            providers::minuteinbox::generate_email()
                .map(|info| with_channel(info, Channel::Mailcatch))
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for minuteinbox")?;
            providers::minuteinbox::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::TempemailCo,
        name: "TempEmail.co",
        website: "tempemail.co",
        generate: Box::new(|duration, domain| providers::tempemail_co::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for tempemail-co")?;
            providers::tempemail_co::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::TempemailsNet,
        name: "TempEmails.net",
        website: "tempemails.net",
        generate: Box::new(|duration, domain| providers::tempemails_net::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for tempemails-net")?;
            providers::tempemails_net::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Altmails,
        name: "AltMails",
        website: "tempmail.altmails.com",
        generate: Box::new(|duration, domain| providers::altmails::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for altmails")?;
            providers::altmails::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::TempemailInfo,
        name: "TempEmailInfo",
        website: "tempemail.info",
        generate: Box::new(|duration, domain| providers::tempemail_info::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for tempemail-info")?;
            providers::tempemail_info::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Smailpro,
        name: "SmailPro",
        website: "smailpro.com",
        generate: Box::new(|duration, domain| providers::smailpro::generate_email()),
        get_emails: Box::new(|email, token| {
            providers::smailpro::get_emails(token.unwrap_or(""), email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::Tempmailten,
        name: "TempMailTen",
        website: "tempmailten.com",
        generate: Box::new(|duration, domain| providers::tempmailten::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for tempmailten")?;
            providers::tempmailten::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::MaildropCc,
        name: "MailDrop.cc",
        website: "maildrop.cc",
        generate: Box::new(|duration, domain| providers::maildrop_cc::generate_email()),
        get_emails: Box::new(|email, token| {
            providers::maildrop_cc::get_emails(token.unwrap_or(""), email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::TenminutemailNet,
        name: "10MinuteMail.net",
        website: "10minutemail.net",
        generate: Box::new(|duration, domain| providers::tenminutemail_net::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for 10minutemail-net")?;
            providers::tenminutemail_net::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::LinshiyouxiangNet,
        name: "LinShiYouXiang",
        website: "linshiyouxiang.net",
        generate: Box::new(|duration, domain| providers::linshiyouxiang_net::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for linshiyouxiang-net")?;
            providers::linshiyouxiang_net::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::TempmailFyi,
        name: "TempMail.FYI",
        website: "temp-mail.fyi",
        generate: Box::new(|duration, domain| providers::tempmail_fyi::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for tempmail-fyi")?;
            providers::tempmail_fyi::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::DisposablemailCom,
        name: "DisposableMail",
        website: "disposablemail.com",
        generate: Box::new(|duration, domain| providers::disposablemail_com::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for disposablemail-com")?;
            providers::disposablemail_com::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::TemppMails,
        name: "TempP-Mails",
        website: "tempp-mails.com",
        generate: Box::new(|duration, domain| providers::tempp_mails::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for tempp-mails")?;
            providers::tempp_mails::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::EmailtempOrg,
        name: "EmailTemp.org",
        website: "emailtemp.org",
        generate: Box::new(|duration, domain| providers::emailtemp_org::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for emailtemp-org")?;
            providers::emailtemp_org::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::MytempmailCc,
        name: "MyTempMail.cc",
        website: "mytempmail.cc",
        generate: Box::new(|duration, domain| providers::mytempmail_cc::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for mytempmail-cc")?;
            providers::mytempmail_cc::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::TempMailNow,
        name: "TempMailNow",
        website: "temp-mail.now",
        generate: Box::new(|duration, domain| providers::temp_mail_now::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for temp-mail-now")?;
            providers::temp_mail_now::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::MailTd,
        name: "Mail.td",
        website: "mail.td",
        generate: Box::new(|duration, domain| providers::mail_td::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for mail-td")?;
            providers::mail_td::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::MailholeDe,
        name: "Mailhole",
        website: "mailhole.de",
        generate: Box::new(|duration, domain| providers::mailhole_de::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for mailhole-de")?;
            providers::mailhole_de::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::TmailLink,
        name: "tmail.link",
        website: "tmail.link",
        generate: Box::new(|duration, domain| providers::tmail_link::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for tmail-link")?;
            providers::tmail_link::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::TwentyfourmailChacuo,
        name: "24mail",
        website: "24mail.chacuo.net",
        generate: Box::new(|duration, domain| providers::twentyfourmail_chacuo::generate_email()),
        get_emails: Box::new(|email, token| providers::twentyfourmail_chacuo::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Nimail,
        name: "NiMail",
        website: "nimail.cn",
        generate: Box::new(|duration, domain| providers::nimail::generate_email()),
        get_emails: Box::new(|email, token| providers::nimail::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Freecustom,
        name: "FreeCustom.Email",
        website: "freecustom.email",
        generate: Box::new(|duration, domain| providers::freecustom::generate_email()),
        get_emails: Box::new(|email, token| providers::freecustom::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::N16888888Cyou,
        name: "Mailmomy (16888888.cyou)",
        website: "mailmomy.com",
        generate: Box::new(|duration, domain| providers::n16888888_cyou::generate_email()),
        get_emails: Box::new(|email, token| providers::n16888888_cyou::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::N17666688Shop,
        name: "Mailmomy (17666688.shop)",
        website: "mailmomy.com",
        generate: Box::new(|duration, domain| providers::n17666688_shop::generate_email()),
        get_emails: Box::new(|email, token| providers::n17666688_shop::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::N282mailCom,
        name: "Mailmomy (282mail.com)",
        website: "mailmomy.com",
        generate: Box::new(|duration, domain| providers::n282mail_com::generate_email()),
        get_emails: Box::new(|email, token| providers::n282mail_com::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::BlackholeDjurbySe,
        name: "Mailinator (blackhole.djurby.se)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::blackhole_djurby_se::generate_email()),
        get_emails: Box::new(|email, token| providers::blackhole_djurby_se::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::BlockBdeaCc,
        name: "Mailinator (block.bdea.cc)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::block_bdea_cc::generate_email()),
        get_emails: Box::new(|email, token| providers::block_bdea_cc::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Bsdu32Buzz,
        name: "Mailmomy (bsdu32.buzz)",
        website: "mailmomy.com",
        generate: Box::new(|duration, domain| providers::bsdu32_buzz::generate_email()),
        get_emails: Box::new(|email, token| providers::bsdu32_buzz::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::BSmellyCc,
        name: "Mailinator (b.smelly.cc)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::b_smelly_cc::generate_email()),
        get_emails: Box::new(|email, token| providers::b_smelly_cc::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Carlton183ChangeipNet,
        name: "Mailinator (183carlton.changeip.net)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::carlton183_changeip_net::generate_email()),
        get_emails: Box::new(|email, token| providers::carlton183_changeip_net::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::DeaSoonIt,
        name: "Mailinator (dea.soon.it)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::dea_soon_it::generate_email()),
        get_emails: Box::new(|email, token| providers::dea_soon_it::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::DisposableAlSudaniCom,
        name: "Mailinator (disposable.al-sudani.com)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| {
            providers::disposable_al_sudani_com::generate_email()
        }),
        get_emails: Box::new(|email, token| providers::disposable_al_sudani_com::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::DisposableNogonadNl,
        name: "Mailinator (disposable.nogonad.nl)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::disposable_nogonad_nl::generate_email()),
        get_emails: Box::new(|email, token| providers::disposable_nogonad_nl::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Doxu243Buzz,
        name: "Mailmomy (doxu243.buzz)",
        website: "mailmomy.com",
        generate: Box::new(|duration, domain| providers::doxu243_buzz::generate_email()),
        get_emails: Box::new(|email, token| providers::doxu243_buzz::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::EasymePro,
        name: "Mailmomy (easyme.pro)",
        website: "mailmomy.com",
        generate: Box::new(|duration, domain| providers::easyme_pro::generate_email()),
        get_emails: Box::new(|email, token| providers::easyme_pro::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::EbsComAr,
        name: "Mailinator (ebs.com.ar)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::ebs_com_ar::generate_email()),
        get_emails: Box::new(|email, token| providers::ebs_com_ar::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::EtgdevDe,
        name: "Mailinator (etgdev.de)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::etgdev_de::generate_email()),
        get_emails: Box::new(|email, token| providers::etgdev_de::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::EvergreencoShop,
        name: "Mailmomy (evergreenco.shop)",
        website: "mailmomy.com",
        generate: Box::new(|duration, domain| providers::evergreenco_shop::generate_email()),
        get_emails: Box::new(|email, token| providers::evergreenco_shop::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Fwd2mEszettEs,
        name: "Mailinator (fwd2m.eszett.es)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::fwd2m_eszett_es::generate_email()),
        get_emails: Box::new(|email, token| providers::fwd2m_eszett_es::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::JamaTrenetEu,
        name: "Mailinator (jama.trenet.eu)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::jama_trenet_eu::generate_email()),
        get_emails: Box::new(|email, token| providers::jama_trenet_eu::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::JFairuseOrg,
        name: "Mailinator (j.fairuse.org)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::j_fairuse_org::generate_email()),
        get_emails: Box::new(|email, token| providers::j_fairuse_org::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::LayuemingPics,
        name: "Mailmomy (layueming.pics)",
        website: "mailmomy.com",
        generate: Box::new(|duration, domain| providers::layueming_pics::generate_email()),
        get_emails: Box::new(|email, token| providers::layueming_pics::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::M887At,
        name: "Mailinator (m.887.at)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::m_887_at::generate_email()),
        get_emails: Box::new(|email, token| providers::m_887_at::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::M8rDavidfuhrDe,
        name: "Mailinator (m8r.davidfuhr.de)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::m8r_davidfuhr_de::generate_email()),
        get_emails: Box::new(|email, token| providers::m8r_davidfuhr_de::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::M8rMcasalCom,
        name: "Mailinator (m8r.mcasal.com)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::m8r_mcasal_com::generate_email()),
        get_emails: Box::new(|email, token| providers::m8r_mcasal_com::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::MailBentraskCom,
        name: "Mailinator (mail.bentrask.com)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::mail_bentrask_com::generate_email()),
        get_emails: Box::new(|email, token| providers::mail_bentrask_com::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::MailFsmashOrg,
        name: "Mailinator (mail.fsmash.org)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::mail_fsmash_org::generate_email()),
        get_emails: Box::new(|email, token| providers::mail_fsmash_org::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::MailinatorzzMoooCom,
        name: "Mailinator (mailinatorzz.mooo.com)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::mailinatorzz_mooo_com::generate_email()),
        get_emails: Box::new(|email, token| providers::mailinatorzz_mooo_com::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::MiMeonBe,
        name: "Mailinator (mi.meon.be)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::mi_meon_be::generate_email()),
        get_emails: Box::new(|email, token| providers::mi_meon_be::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::MingyuekejiOnline,
        name: "Mailmomy (mingyuekeji.online)",
        website: "mailmomy.com",
        generate: Box::new(|duration, domain| providers::mingyuekeji_online::generate_email()),
        get_emails: Box::new(|email, token| providers::mingyuekeji_online::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::MingyuemingClick,
        name: "Mailmomy (mingyueming.click)",
        website: "mailmomy.com",
        generate: Box::new(|duration, domain| providers::mingyueming_click::generate_email()),
        get_emails: Box::new(|email, token| providers::mingyueming_click::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::MingyuemingShop,
        name: "Mailmomy (mingyueming.shop)",
        website: "mailmomy.com",
        generate: Box::new(|duration, domain| providers::mingyueming_shop::generate_email()),
        get_emails: Box::new(|email, token| providers::mingyueming_shop::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::MingyukejiLol,
        name: "Mailmomy (mingyukeji.lol)",
        website: "mailmomy.com",
        generate: Box::new(|duration, domain| providers::mingyukeji_lol::generate_email()),
        get_emails: Box::new(|email, token| providers::mingyukeji_lol::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::MnCurppaCom,
        name: "Mailinator (mn.curppa.com)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::mn_curppa_com::generate_email()),
        get_emails: Box::new(|email, token| providers::mn_curppa_com::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::MNikMe,
        name: "Mailinator (m.nik.me)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::m_nik_me::generate_email()),
        get_emails: Box::new(|email, token| providers::m_nik_me::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::MtmdevCom,
        name: "Mailinator (mtmdev.com)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::mtmdev_com::generate_email()),
        get_emails: Box::new(|email, token| providers::mtmdev_com::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::NospamThurstonsUs,
        name: "Mailinator (nospam.thurstons.us)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::nospam_thurstons_us::generate_email()),
        get_emails: Box::new(|email, token| providers::nospam_thurstons_us::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Notfond404Mn,
        name: "Mailinator (notfond.404.mn)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::notfond_404_mn::generate_email()),
        get_emails: Box::new(|email, token| providers::notfond_404_mn::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::NullK3vinNet,
        name: "Mailinator (null.k3vin.net)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::null_k3vin_net::generate_email()),
        get_emails: Box::new(|email, token| providers::null_k3vin_net::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Nuxh62Space,
        name: "Mailmomy (nuxh62.space)",
        website: "mailmomy.com",
        generate: Box::new(|duration, domain| providers::nuxh62_space::generate_email()),
        get_emails: Box::new(|email, token| providers::nuxh62_space::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::ProidCloudIpCc,
        name: "Mailmomy (proid.cloud-ip.cc)",
        website: "mailmomy.com",
        generate: Box::new(|duration, domain| providers::proid_cloud_ip_cc::generate_email()),
        get_emails: Box::new(|email, token| providers::proid_cloud_ip_cc::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::RamjaneMoooCom,
        name: "Mailinator (ramjane.mooo.com)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::ramjane_mooo_com::generate_email()),
        get_emails: Box::new(|email, token| providers::ramjane_mooo_com::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::RauxaSenyCat,
        name: "Mailinator (rauxa.seny.cat)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::rauxa_seny_cat::generate_email()),
        get_emails: Box::new(|email, token| providers::rauxa_seny_cat::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::ReallyIstrashCom,
        name: "Mailinator (really.istrash.com)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::really_istrash_com::generate_email()),
        get_emails: Box::new(|email, token| providers::really_istrash_com::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::SbookPics,
        name: "Mailmomy (sbook.pics)",
        website: "mailmomy.com",
        generate: Box::new(|duration, domain| providers::sbook_pics::generate_email()),
        get_emails: Box::new(|email, token| providers::sbook_pics::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::SpamHortukOvh,
        name: "Mailinator (spam.hortuk.ovh)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::spam_hortuk_ovh::generate_email()),
        get_emails: Box::new(|email, token| providers::spam_hortuk_ovh::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::SpWootAt,
        name: "Mailinator (sp.woot.at)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::sp_woot_at::generate_email()),
        get_emails: Box::new(|email, token| providers::sp_woot_at::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::TestUnergieCom,
        name: "Mailinator (test.unergie.com)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::test_unergie_com::generate_email()),
        get_emails: Box::new(|email, token| providers::test_unergie_com::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::TorchYiOrg,
        name: "Mailinator (torch.yi.org)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::torch_yi_org::generate_email()),
        get_emails: Box::new(|email, token| providers::torch_yi_org::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::TZibetNet,
        name: "Mailinator (t.zibet.net)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::t_zibet_net::generate_email()),
        get_emails: Box::new(|email, token| providers::t_zibet_net::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Xue32Buzz,
        name: "Mailmomy (xue32.buzz)",
        website: "mailmomy.com",
        generate: Box::new(|duration, domain| providers::xue32_buzz::generate_email()),
        get_emails: Box::new(|email, token| providers::xue32_buzz::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Apihz,
        name: "ApiHz TempMail",
        website: "apihz.cn",
        generate: Box::new(|duration, domain| providers::apihz::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for apihz")?;
            providers::apihz::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::SogetthisCom,
        name: "Mailinator (sogetthis.com)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::sogetthis_com::generate_email()),
        get_emails: Box::new(|email, token| providers::sogetthis_com::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::BobmailInfo,
        name: "Mailinator (bobmail.info)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::bobmail_info::generate_email()),
        get_emails: Box::new(|email, token| providers::bobmail_info::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::SuremailInfo,
        name: "Mailinator (suremail.info)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::suremail_info::generate_email()),
        get_emails: Box::new(|email, token| providers::suremail_info::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::BinkmailCom,
        name: "Mailinator (binkmail.com)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::binkmail_com::generate_email()),
        get_emails: Box::new(|email, token| providers::binkmail_com::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::VeryrealemailCom,
        name: "Mailinator (veryrealemail.com)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::veryrealemail_com::generate_email()),
        get_emails: Box::new(|email, token| providers::veryrealemail_com::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Mailmomy,
        name: "Mailmomy",
        website: "mailmomy.com",
        generate: Box::new(|duration, domain| providers::mailmomy::generate_email()),
        get_emails: Box::new(|email, token| providers::mailmomy::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::ChammyInfo,
        name: "Mailinator (chammy.info)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::chammy_info::generate_email()),
        get_emails: Box::new(|email, token| providers::chammy_info::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::ThisisnotmyrealemailCom,
        name: "Mailinator (thisisnotmyrealemail.com)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| {
            providers::thisisnotmyrealemail_com::generate_email()
        }),
        get_emails: Box::new(|email, token| providers::thisisnotmyrealemail_com::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::NotmailinatorCom,
        name: "Mailinator (notmailinator.com)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::notmailinator_com::generate_email()),
        get_emails: Box::new(|email, token| providers::notmailinator_com::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::SpamherepleaseCom,
        name: "Mailinator (spamhereplease.com)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::spamhereplease_com::generate_email()),
        get_emails: Box::new(|email, token| providers::spamhereplease_com::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::SendspamhereCom,
        name: "Mailinator (sendspamhere.com)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::sendspamhere_com::generate_email()),
        get_emails: Box::new(|email, token| providers::sendspamhere_com::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::SendfreeOrg,
        name: "Mailinator (sendfree.org)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::sendfree_org::generate_email()),
        get_emails: Box::new(|email, token| providers::sendfree_org::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::JunkBeatsOrg,
        name: "Mailinator (junk.beats.org)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::junk_beats_org::generate_email()),
        get_emails: Box::new(|email, token| providers::junk_beats_org::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::JunkIhmehlCom,
        name: "Mailinator (junk.ihmehl.com)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::junk_ihmehl_com::generate_email()),
        get_emails: Box::new(|email, token| providers::junk_ihmehl_com::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::JunkNoplayOrg,
        name: "Mailinator (junk.noplay.org)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::junk_noplay_org::generate_email()),
        get_emails: Box::new(|email, token| providers::junk_noplay_org::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::JunkVanillasystemCom,
        name: "Mailinator (junk.vanillasystem.com)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::junk_vanillasystem_com::generate_email()),
        get_emails: Box::new(|email, token| providers::junk_vanillasystem_com::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::SpamJasonpearceCom,
        name: "Mailinator (spam.jasonpearce.com)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::spam_jasonpearce_com::generate_email()),
        get_emails: Box::new(|email, token| providers::spam_jasonpearce_com::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::FishSkytaleNet,
        name: "Mailinator (fish.skytale.net)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::fish_skytale_net::generate_email()),
        get_emails: Box::new(|email, token| providers::fish_skytale_net::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::SpamMccrewCom,
        name: "Mailinator (spam.mccrew.com)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::spam_mccrew_com::generate_email()),
        get_emails: Box::new(|email, token| providers::spam_mccrew_com::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::DropmailClick,
        name: "DropMail.click",
        website: "dropmail.click",
        generate: Box::new(|duration, domain| providers::dropmail_click::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for dropmail-click")?;
            providers::dropmail_click::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::SpamCoroiuCom,
        name: "Mailinator (spam.coroiu.com)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::spam_coroiu_com::generate_email()),
        get_emails: Box::new(|email, token| providers::spam_coroiu_com::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::SpamDeluserNet,
        name: "Mailinator (spam.deluser.net)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::spam_deluser_net::generate_email()),
        get_emails: Box::new(|email, token| providers::spam_deluser_net::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::SpamDhsfNet,
        name: "Mailinator (spam.dhsf.net)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::spam_dhsf_net::generate_email()),
        get_emails: Box::new(|email, token| providers::spam_dhsf_net::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::SpamLucatntCom,
        name: "Mailinator (spam.lucatnt.com)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::spam_lucatnt_com::generate_email()),
        get_emails: Box::new(|email, token| providers::spam_lucatnt_com::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::SpamLyceumLifeComRu,
        name: "Mailinator (spam.lyceum-life.com.ru)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::spam_lyceum_life_com_ru::generate_email()),
        get_emails: Box::new(|email, token| providers::spam_lyceum_life_com_ru::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::SpamNetpiratesNet,
        name: "Mailinator (spam.netpirates.net)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::spam_netpirates_net::generate_email()),
        get_emails: Box::new(|email, token| providers::spam_netpirates_net::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::SpamNoIpNet,
        name: "Mailinator (spam.no-ip.net)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::spam_no_ip_net::generate_email()),
        get_emails: Box::new(|email, token| providers::spam_no_ip_net::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::SpamOzhOrg,
        name: "Mailinator (spam.ozh.org)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::spam_ozh_org::generate_email()),
        get_emails: Box::new(|email, token| providers::spam_ozh_org::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::SpamPyphusOrg,
        name: "Mailinator (spam.pyphus.org)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::spam_pyphus_org::generate_email()),
        get_emails: Box::new(|email, token| providers::spam_pyphus_org::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::SpamShepPw,
        name: "Mailinator (spam.shep.pw)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::spam_shep_pw::generate_email()),
        get_emails: Box::new(|email, token| providers::spam_shep_pw::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::SpamWtfAt,
        name: "Mailinator (spam.wtf.at)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::spam_wtf_at::generate_email()),
        get_emails: Box::new(|email, token| providers::spam_wtf_at::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::SpamWulczerOrg,
        name: "Mailinator (spam.wulczer.org)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::spam_wulczer_org::generate_email()),
        get_emails: Box::new(|email, token| providers::spam_wulczer_org::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::CrapKakaduaNet,
        name: "Mailinator (crap.kakadua.net)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::crap_kakadua_net::generate_email()),
        get_emails: Box::new(|email, token| providers::crap_kakadua_net::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::SpamJanlugtNl,
        name: "Mailinator (spam.janlugt.nl)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::spam_janlugt_nl::generate_email()),
        get_emails: Box::new(|email, token| providers::spam_janlugt_nl::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::MinBurningfishNet,
        name: "Mailinator (min.burningfish.net)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::min_burningfish_net::generate_email()),
        get_emails: Box::new(|email, token| providers::min_burningfish_net::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::SinkFblayCom,
        name: "Mailinator (sink.fblay.com)",
        website: "mailinator.com",
        generate: Box::new(|duration, domain| providers::sink_fblay_com::generate_email()),
        get_emails: Box::new(|email, token| providers::sink_fblay_com::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::Tempgmailer,
        name: "TempGmailer",
        website: "tempgmailer.com",
        generate: Box::new(|duration, domain| providers::tempgmailer::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for tempgmailer")?;
            providers::tempgmailer::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::TempMailOrg,
        name: "Temp-Mail.org",
        website: "temp-mail.org",
        generate: Box::new(|duration, domain| {
            providers::temp_mail_org::generate_email(duration, domain)
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for temp-mail-org")?;
            providers::temp_mail_org::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::XkxMe,
        name: "XKX.me",
        website: "xkx.me",
        generate: Box::new(|duration, domain| providers::xkx_me::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for xkx-me")?;
            providers::xkx_me::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::GoneboxEmail,
        name: "Gonebox Email",
        website: "gonebox.email",
        generate: Box::new(|duration, domain| providers::gonebox_email::generate_email()),
        get_emails: Box::new(|email, token| providers::gonebox_email::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::MailcatAi,
        name: "Mailcat AI",
        website: "mailcat.ai",
        generate: Box::new(|duration, domain| providers::mailcat_ai::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for mailcat-ai")?;
            providers::mailcat_ai::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::TempgoEmail,
        name: "TempGo Email",
        website: "tempgo.email",
        generate: Box::new(|duration, domain| providers::tempgo_email::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for tempgo-email")?;
            providers::tempgo_email::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::RestmailNet,
        name: "Restmail.net",
        website: "restmail.net",
        generate: Box::new(|duration, domain| providers::restmail_net::generate_email()),
        get_emails: Box::new(|email, token| providers::restmail_net::get_emails(email)),
    });

    reg.push(ChannelSpec {
        channel: Channel::DropmailMe,
        name: "DropMail.me",
        website: "dropmail.me",
        generate: Box::new(|duration, domain| {
            providers::dropmail_me::generate_email(duration, domain)
        }),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for dropmail-me")?;
            providers::dropmail_me::get_emails(t, email)
        }),
    });

    reg.push(ChannelSpec {
        channel: Channel::TenMinuteMailNet,
        name: "10MinuteMail.net",
        website: "10minutemail.net",
        generate: Box::new(|duration, domain| providers::ten_minute_mail_net::generate_email()),
        get_emails: Box::new(|email, token| {
            let t = token.ok_or("token is required for ten-minute-mail-net")?;
            providers::ten_minute_mail_net::get_emails(t, email)
        }),
    });

    reg
}

/// 全局有序注册表（惰性构造并缓存）。
fn registry() -> &'static Vec<ChannelSpec> {
    static REGISTRY: OnceLock<Vec<ChannelSpec>> = OnceLock::new();
    REGISTRY.get_or_init(build_registry)
}

/// 渠道标识到注册表下标的映射（O(1) 分发查找）。
fn registry_index() -> &'static HashMap<Channel, usize> {
    static INDEX: OnceLock<HashMap<Channel, usize>> = OnceLock::new();
    INDEX.get_or_init(|| {
        let mut m = HashMap::new();
        for (i, spec) in registry().iter().enumerate() {
            if m.insert(spec.channel.clone(), i).is_some() {
                panic!("duplicate channel registration: {}", spec.channel);
            }
        }
        m
    })
}

/// 按注册顺序返回的全部渠道有序列表（缓存）。
fn channel_order() -> &'static Vec<Channel> {
    static ORDER: OnceLock<Vec<Channel>> = OnceLock::new();
    ORDER.get_or_init(|| registry().iter().map(|s| s.channel.clone()).collect())
}

/// 所有支持的渠道有序切片，供随机选择、遍历与派生使用。
/// 顺序即注册顺序（硬约束，五端一致）。
pub fn all_channels() -> &'static [Channel] {
    channel_order().as_slice()
}

/// 查找指定渠道的注册规格。
pub(crate) fn spec_for(channel: &Channel) -> Option<&'static ChannelSpec> {
    registry_index().get(channel).map(|&i| &registry()[i])
}

/// 获取指定渠道的详细信息（由注册表派生）。
pub fn get_channel_info(channel: &Channel) -> Option<ChannelInfo> {
    spec_for(channel).map(|s| ChannelInfo {
        channel: s.channel.clone(),
        name: s.name,
        website: s.website,
    })
}

/// 获取所有支持的渠道信息列表（由注册表派生，顺序与 all_channels 一致）。
pub fn list_channels() -> Vec<ChannelInfo> {
    registry()
        .iter()
        .map(|s| ChannelInfo {
            channel: s.channel.clone(),
            name: s.name,
            website: s.website,
        })
        .collect()
}
