/*!
 * SDK 主入口
 * 聚合所有渠道的逻辑，提供统一的 API
 */

use crate::providers;
use crate::retry::with_retry_with_attempts;
use crate::telemetry::report_telemetry;
use crate::types::*;

/// 所有支持的公共渠道列表；随机尝试顺序会基于该列表在本端独立洗牌，不要求跨 SDK 一致
pub const ALL_CHANNELS: &[Channel] = &[
    Channel::Tempmail,
    Channel::TempmailCn,
    Channel::TaEasy,
    Channel::TenminuteOne,
    Channel::XghffCom,
    Channel::OqqajCom,
    Channel::PsovvCom,
    Channel::DbwotCom,
    Channel::YgwprCom,
    Channel::ImxweCom,
    Channel::Linshiyou,
    Channel::Mffac,
    Channel::TempmailLol,
    Channel::ChatgptOrgUk,
    Channel::TempMailIo,
    Channel::MailCx,
    Channel::DdkerCom,
    Channel::Catchmail,
    Channel::CatchmailMailistry,
    Channel::CatchmailZeppost,
    Channel::Mailforspam,
    Channel::MailforspamTempmailIo,
    Channel::MailforspamDisposable,
    Channel::Tempmailc,
    Channel::Mailnesia,
    Channel::Throwawaymail,
    Channel::ShittyEmail,
    Channel::Tempmailpro,
    Channel::DevmailUk,
    Channel::CleanTempMail,
    Channel::Inboxkitten,
    Channel::Getnada,
    Channel::OneVpnNet,
    Channel::AbematvCom,
    Channel::AbematvNet,
    Channel::AbematvOrg,
    Channel::AcehCc,
    Channel::BangkabelitungNet,
    Channel::CctruyenCom,
    Channel::GetnadaCom,
    Channel::GetnadaEmail,
    Channel::GetnadaNet,
    Channel::JawatengahNet,
    Channel::JawatimurNet,
    Channel::KalimantanbaratNet,
    Channel::KalimantanselatanNet,
    Channel::KalimantantengahNet,
    Channel::KalimantantimurNet,
    Channel::KalimantanutaraNet,
    Channel::KepulauanriauNet,
    Channel::Luxury345Com,
    Channel::MalukuutaraNet,
    Channel::NusatenggarabaratNet,
    Channel::NusatenggaratimurNet,
    Channel::PapuabaratNet,
    Channel::PapuabaratdayaNet,
    Channel::PapuaselatanNet,
    Channel::PeholCom,
    Channel::PtruyenCom,
    Channel::PulaubaliNet,
    Channel::RiauNet,
    Channel::SeokeyOrg,
    Channel::SulawesibaratNet,
    Channel::SulawesiselatanNet,
    Channel::SulawesitengahNet,
    Channel::SulawesitenggaraNet,
    Channel::SumaterabaratNet,
    Channel::SumateraselatanNet,
    Channel::SumaterautaraNet,
    Channel::VillatogelCom,
    Channel::Mail123,
    Channel::Mail10s,
    Channel::Webmailtemp,
    Channel::Tempfastmail,
    Channel::OneSecMail,
    Channel::Fakemail,
    Channel::Openinbox,
    Channel::Inboxes,
    Channel::Uncorreotemporal,
    Channel::Awamail,
    Channel::MailTm,
    Channel::WebLibraryNet,
    Channel::Dropmail,
    Channel::GuerrillaMail,
    Channel::GuerrillamailCom,
    Channel::Maildrop,
    Channel::SmailPw,
    Channel::Vip215,
    Channel::FakeLegal,
    Channel::ImguiDe,
    Channel::PulsewebmenuDe,
    Channel::Moakt,
    Channel::DrmailIn,
    Channel::TemlNet,
    Channel::TmpemlCom,
    Channel::TmpboxNet,
    Channel::MoaktCc,
    Channel::DisboxNet,
    Channel::TmpmailOrg,
    Channel::TmpmailNet,
    Channel::TmailsNet,
    Channel::DisboxOrg,
    Channel::MoaktCo,
    Channel::MoaktWs,
    Channel::TmailWs,
    Channel::BareedWs,
    Channel::Email10min,
    Channel::MjjCm,
    Channel::LinshiCo,
    Channel::Harakirimail,
    Channel::JqkjqkXyz,
    Channel::LyhleviCom,
    Channel::TempmailPlus,
    Channel::FexpostCom,
    Channel::FexboxOrg,
    Channel::MailboxInUa,
    Channel::RoverInfo,
    Channel::ChitthiIn,
    Channel::FextempCom,
    Channel::AnyPink,
    Channel::MerepostCom,
    Channel::TempmailLolV2,
    Channel::Tempgbox,
    Channel::Emailnator,
    Channel::Temporam,
    Channel::Neighbours,
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
    Channel::M2u,
    Channel::TempyEmail,
    Channel::Fmail,
    Channel::Ockito,
    Channel::Anonbox,
    Channel::Mailinator,
    Channel::Duckmail,
    Channel::Tempmail365,
    Channel::Tempinbox,
    Channel::Byom,
    Channel::Anonymmail,
    Channel::Eyepaste,
    Channel::Segamail,
    Channel::MailSunls,
    Channel::Expressinboxhub,
    Channel::Lroid,
    Channel::Haribu,
    Channel::Pleasenospam,
    Channel::Rootsh,
    Channel::FakeEmailSite,
    Channel::Mohmal,
    Channel::Mailgolem,
    Channel::BestTempMail,
    Channel::DisposablemailApp,
    Channel::MailtempCc,
    Channel::Minuteinbox,
    Channel::Mailcatch,
    Channel::TempemailCo,
    Channel::TempemailsNet,
    Channel::Altmails,
    Channel::TempemailInfo,
    Channel::Smailpro,
    Channel::Tempmailten,
    Channel::MaildropCc,
    Channel::TenminutemailNet,
    Channel::LinshiyouxiangNet,
    Channel::TempmailFyi,
    Channel::DisposablemailCom,
    Channel::TemppMails,
    Channel::EmailtempOrg,
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
        Channel::XghffCom => ChannelInfo {
            channel: Channel::XghffCom,
            name: "xghff.com",
            website: "10minutemail.one",
        },
        Channel::OqqajCom => ChannelInfo {
            channel: Channel::OqqajCom,
            name: "oqqaj.com",
            website: "10minutemail.one",
        },
        Channel::PsovvCom => ChannelInfo {
            channel: Channel::PsovvCom,
            name: "psovv.com",
            website: "10minutemail.one",
        },
        Channel::DbwotCom => ChannelInfo {
            channel: Channel::DbwotCom,
            name: "dbwot.com",
            website: "10minutemail.one",
        },
        Channel::YgwprCom => ChannelInfo {
            channel: Channel::YgwprCom,
            name: "ygwpr.com",
            website: "10minutemail.one",
        },
        Channel::ImxweCom => ChannelInfo {
            channel: Channel::ImxweCom,
            name: "imxwe.com",
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
        Channel::DdkerCom => ChannelInfo {
            channel: Channel::DdkerCom,
            name: "ddker.com",
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
        Channel::ShittyEmail => ChannelInfo {
            channel: Channel::ShittyEmail,
            name: "shitty.email",
            website: "shitty.email",
        },
        Channel::Tempmailpro => ChannelInfo {
            channel: Channel::Tempmailpro,
            name: "TempMail Pro",
            website: "tempmailpro.us",
        },
        Channel::DevmailUk => ChannelInfo {
            channel: Channel::DevmailUk,
            name: "DevMail UK",
            website: "devmail.uk",
        },
        Channel::CleanTempMail => ChannelInfo {
            channel: Channel::CleanTempMail,
            name: "CleanTempMail",
            website: "cleantempmail.com",
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
        Channel::OneVpnNet => ChannelInfo {
            channel: Channel::OneVpnNet,
            name: "1vpn.net",
            website: "getnada.net",
        },
        Channel::AbematvCom => ChannelInfo {
            channel: Channel::AbematvCom,
            name: "abematv.com",
            website: "getnada.net",
        },
        Channel::AbematvNet => ChannelInfo {
            channel: Channel::AbematvNet,
            name: "abematv.net",
            website: "getnada.net",
        },
        Channel::AbematvOrg => ChannelInfo {
            channel: Channel::AbematvOrg,
            name: "abematv.org",
            website: "getnada.net",
        },
        Channel::AcehCc => ChannelInfo {
            channel: Channel::AcehCc,
            name: "aceh.cc",
            website: "getnada.net",
        },
        Channel::BangkabelitungNet => ChannelInfo {
            channel: Channel::BangkabelitungNet,
            name: "bangkabelitung.net",
            website: "getnada.net",
        },
        Channel::CctruyenCom => ChannelInfo {
            channel: Channel::CctruyenCom,
            name: "cctruyen.com",
            website: "getnada.net",
        },
        Channel::GetnadaCom => ChannelInfo {
            channel: Channel::GetnadaCom,
            name: "getnada.com",
            website: "getnada.net",
        },
        Channel::GetnadaEmail => ChannelInfo {
            channel: Channel::GetnadaEmail,
            name: "getnada.email",
            website: "getnada.net",
        },
        Channel::GetnadaNet => ChannelInfo {
            channel: Channel::GetnadaNet,
            name: "getnada.net",
            website: "getnada.net",
        },
        Channel::JawatengahNet => ChannelInfo {
            channel: Channel::JawatengahNet,
            name: "jawatengah.net",
            website: "getnada.net",
        },
        Channel::JawatimurNet => ChannelInfo {
            channel: Channel::JawatimurNet,
            name: "jawatimur.net",
            website: "getnada.net",
        },
        Channel::KalimantanbaratNet => ChannelInfo {
            channel: Channel::KalimantanbaratNet,
            name: "kalimantanbarat.net",
            website: "getnada.net",
        },
        Channel::KalimantanselatanNet => ChannelInfo {
            channel: Channel::KalimantanselatanNet,
            name: "kalimantanselatan.net",
            website: "getnada.net",
        },
        Channel::KalimantantengahNet => ChannelInfo {
            channel: Channel::KalimantantengahNet,
            name: "kalimantantengah.net",
            website: "getnada.net",
        },
        Channel::KalimantantimurNet => ChannelInfo {
            channel: Channel::KalimantantimurNet,
            name: "kalimantantimur.net",
            website: "getnada.net",
        },
        Channel::KalimantanutaraNet => ChannelInfo {
            channel: Channel::KalimantanutaraNet,
            name: "kalimantanutara.net",
            website: "getnada.net",
        },
        Channel::KepulauanriauNet => ChannelInfo {
            channel: Channel::KepulauanriauNet,
            name: "kepulauanriau.net",
            website: "getnada.net",
        },
        Channel::Luxury345Com => ChannelInfo {
            channel: Channel::Luxury345Com,
            name: "luxury345.com",
            website: "getnada.net",
        },
        Channel::MalukuutaraNet => ChannelInfo {
            channel: Channel::MalukuutaraNet,
            name: "malukuutara.net",
            website: "getnada.net",
        },
        Channel::NusatenggarabaratNet => ChannelInfo {
            channel: Channel::NusatenggarabaratNet,
            name: "nusatenggarabarat.net",
            website: "getnada.net",
        },
        Channel::NusatenggaratimurNet => ChannelInfo {
            channel: Channel::NusatenggaratimurNet,
            name: "nusatenggaratimur.net",
            website: "getnada.net",
        },
        Channel::PapuabaratNet => ChannelInfo {
            channel: Channel::PapuabaratNet,
            name: "papuabarat.net",
            website: "getnada.net",
        },
        Channel::PapuabaratdayaNet => ChannelInfo {
            channel: Channel::PapuabaratdayaNet,
            name: "papuabaratdaya.net",
            website: "getnada.net",
        },
        Channel::PapuaselatanNet => ChannelInfo {
            channel: Channel::PapuaselatanNet,
            name: "papuaselatan.net",
            website: "getnada.net",
        },
        Channel::PeholCom => ChannelInfo {
            channel: Channel::PeholCom,
            name: "pehol.com",
            website: "getnada.net",
        },
        Channel::PtruyenCom => ChannelInfo {
            channel: Channel::PtruyenCom,
            name: "ptruyen.com",
            website: "getnada.net",
        },
        Channel::PulaubaliNet => ChannelInfo {
            channel: Channel::PulaubaliNet,
            name: "pulaubali.net",
            website: "getnada.net",
        },
        Channel::RiauNet => ChannelInfo {
            channel: Channel::RiauNet,
            name: "riau.net",
            website: "getnada.net",
        },
        Channel::SeokeyOrg => ChannelInfo {
            channel: Channel::SeokeyOrg,
            name: "seokey.org",
            website: "getnada.net",
        },
        Channel::SulawesibaratNet => ChannelInfo {
            channel: Channel::SulawesibaratNet,
            name: "sulawesibarat.net",
            website: "getnada.net",
        },
        Channel::SulawesiselatanNet => ChannelInfo {
            channel: Channel::SulawesiselatanNet,
            name: "sulawesiselatan.net",
            website: "getnada.net",
        },
        Channel::SulawesitengahNet => ChannelInfo {
            channel: Channel::SulawesitengahNet,
            name: "sulawesitengah.net",
            website: "getnada.net",
        },
        Channel::SulawesitenggaraNet => ChannelInfo {
            channel: Channel::SulawesitenggaraNet,
            name: "sulawesitenggara.net",
            website: "getnada.net",
        },
        Channel::SumaterabaratNet => ChannelInfo {
            channel: Channel::SumaterabaratNet,
            name: "sumaterabarat.net",
            website: "getnada.net",
        },
        Channel::SumateraselatanNet => ChannelInfo {
            channel: Channel::SumateraselatanNet,
            name: "sumateraselatan.net",
            website: "getnada.net",
        },
        Channel::SumaterautaraNet => ChannelInfo {
            channel: Channel::SumaterautaraNet,
            name: "sumaterautara.net",
            website: "getnada.net",
        },
        Channel::VillatogelCom => ChannelInfo {
            channel: Channel::VillatogelCom,
            name: "villatogel.com",
            website: "getnada.net",
        },
        Channel::Mail123 => ChannelInfo {
            channel: Channel::Mail123,
            name: "Mail123",
            website: "mail123.fr",
        },
        Channel::Mail10s => ChannelInfo {
            channel: Channel::Mail10s,
            name: "Mail10s",
            website: "mail10s.com",
        },
        Channel::Webmailtemp => ChannelInfo {
            channel: Channel::Webmailtemp,
            name: "WebMailTemp",
            website: "webmailtemp.com",
        },
        Channel::Tempfastmail => ChannelInfo {
            channel: Channel::Tempfastmail,
            name: "TempFastMail",
            website: "tempfastmail.com",
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
        Channel::WebLibraryNet => ChannelInfo {
            channel: Channel::WebLibraryNet,
            name: "web-library.net",
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
        Channel::ImguiDe => ChannelInfo {
            channel: Channel::ImguiDe,
            name: "imgui.de",
            website: "fake.legal",
        },
        Channel::PulsewebmenuDe => ChannelInfo {
            channel: Channel::PulsewebmenuDe,
            name: "pulsewebmenu.de",
            website: "fake.legal",
        },
        Channel::Moakt => ChannelInfo {
            channel: Channel::Moakt,
            name: "Moakt",
            website: "moakt.com",
        },
        Channel::DrmailIn => ChannelInfo {
            channel: Channel::DrmailIn,
            name: "drmail.in",
            website: "moakt.com",
        },
        Channel::TemlNet => ChannelInfo {
            channel: Channel::TemlNet,
            name: "teml.net",
            website: "moakt.com",
        },
        Channel::TmpemlCom => ChannelInfo {
            channel: Channel::TmpemlCom,
            name: "tmpeml.com",
            website: "moakt.com",
        },
        Channel::TmpboxNet => ChannelInfo {
            channel: Channel::TmpboxNet,
            name: "tmpbox.net",
            website: "moakt.com",
        },
        Channel::MoaktCc => ChannelInfo {
            channel: Channel::MoaktCc,
            name: "moakt.cc",
            website: "moakt.com",
        },
        Channel::DisboxNet => ChannelInfo {
            channel: Channel::DisboxNet,
            name: "disbox.net",
            website: "moakt.com",
        },
        Channel::TmpmailOrg => ChannelInfo {
            channel: Channel::TmpmailOrg,
            name: "tmpmail.org",
            website: "moakt.com",
        },
        Channel::TmpmailNet => ChannelInfo {
            channel: Channel::TmpmailNet,
            name: "tmpmail.net",
            website: "moakt.com",
        },
        Channel::TmailsNet => ChannelInfo {
            channel: Channel::TmailsNet,
            name: "tmails.net",
            website: "moakt.com",
        },
        Channel::DisboxOrg => ChannelInfo {
            channel: Channel::DisboxOrg,
            name: "disbox.org",
            website: "moakt.com",
        },
        Channel::MoaktCo => ChannelInfo {
            channel: Channel::MoaktCo,
            name: "moakt.co",
            website: "moakt.com",
        },
        Channel::MoaktWs => ChannelInfo {
            channel: Channel::MoaktWs,
            name: "moakt.ws",
            website: "moakt.com",
        },
        Channel::TmailWs => ChannelInfo {
            channel: Channel::TmailWs,
            name: "tmail.ws",
            website: "moakt.com",
        },
        Channel::BareedWs => ChannelInfo {
            channel: Channel::BareedWs,
            name: "bareed.ws",
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
        Channel::FexpostCom => ChannelInfo {
            channel: Channel::FexpostCom,
            name: "fexpost.com",
            website: "tempmail.plus",
        },
        Channel::FexboxOrg => ChannelInfo {
            channel: Channel::FexboxOrg,
            name: "fexbox.org",
            website: "tempmail.plus",
        },
        Channel::MailboxInUa => ChannelInfo {
            channel: Channel::MailboxInUa,
            name: "mailbox.in.ua",
            website: "tempmail.plus",
        },
        Channel::RoverInfo => ChannelInfo {
            channel: Channel::RoverInfo,
            name: "rover.info",
            website: "tempmail.plus",
        },
        Channel::ChitthiIn => ChannelInfo {
            channel: Channel::ChitthiIn,
            name: "chitthi.in",
            website: "tempmail.plus",
        },
        Channel::FextempCom => ChannelInfo {
            channel: Channel::FextempCom,
            name: "fextemp.com",
            website: "tempmail.plus",
        },
        Channel::AnyPink => ChannelInfo {
            channel: Channel::AnyPink,
            name: "any.pink",
            website: "tempmail.plus",
        },
        Channel::MerepostCom => ChannelInfo {
            channel: Channel::MerepostCom,
            name: "merepost.com",
            website: "tempmail.plus",
        },
        Channel::TempmailLolV2 => ChannelInfo {
            channel: Channel::TempmailLolV2,
            name: "TempMail LOL V2",
            website: "tempmail.lol",
        },
        Channel::Tempgbox => ChannelInfo {
            channel: Channel::Tempgbox,
            name: "TempGBox",
            website: "tempgbox.net",
        },
        Channel::Emailnator => ChannelInfo {
            channel: Channel::Emailnator,
            name: "Emailnator",
            website: "emailnator.com",
        },
        Channel::Temporam => ChannelInfo {
            channel: Channel::Temporam,
            name: "Temporam",
            website: "temporam.com",
        },
        Channel::Neighbours => ChannelInfo {
            channel: Channel::Neighbours,
            name: "Neighbours",
            website: "neighbours.sh",
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
        Channel::JqkjqkXyz => ChannelInfo {
            channel: Channel::JqkjqkXyz,
            name: "jqkjqk.xyz",
            website: "mail.zhujump.com",
        },
        Channel::LyhleviCom => ChannelInfo {
            channel: Channel::LyhleviCom,
            name: "LyhLevi MoeMail",
            website: "lyhlevi.com",
        },
        Channel::M2u => ChannelInfo {
            channel: Channel::M2u,
            name: "MailToYou",
            website: "m2u.io",
        },
        Channel::TempyEmail => ChannelInfo {
            channel: Channel::TempyEmail,
            name: "Tempy Email",
            website: "tempy.email",
        },
        Channel::Fmail => ChannelInfo {
            channel: Channel::Fmail,
            name: "FMail",
            website: "fmail.men",
        },
        Channel::Ockito => ChannelInfo {
            channel: Channel::Ockito,
            name: "Ockito",
            website: "ockito.com",
        },
        Channel::Anonbox => ChannelInfo {
            channel: Channel::Anonbox,
            name: "Anonbox",
            website: "anonbox.net",
        },
        Channel::Mailinator => ChannelInfo {
            channel: Channel::Mailinator,
            name: "Mailinator",
            website: "mailinator.com",
        },
        Channel::Duckmail => ChannelInfo {
            channel: Channel::Duckmail,
            name: "DuckMail",
            website: "duckmail.sbs",
        },
        Channel::Tempmail365 => ChannelInfo {
            channel: Channel::Tempmail365,
            name: "Tempmail365",
            website: "https://tempmail365.cn",
        },
        Channel::Tempinbox => ChannelInfo {
            channel: Channel::Tempinbox,
            name: "TempInbox",
            website: "https://www.tempinbox.xyz",
        },
        Channel::Byom => ChannelInfo {
            channel: Channel::Byom,
            name: "Byom",
            website: "byom.de",
        },
        Channel::Anonymmail => ChannelInfo {
            channel: Channel::Anonymmail,
            name: "AnonymMail",
            website: "anonymmail.net",
        },
        Channel::Eyepaste => ChannelInfo {
            channel: Channel::Eyepaste,
            name: "EyePaste",
            website: "eyepaste.com",
        },
        Channel::Segamail => ChannelInfo {
            channel: Channel::Segamail,
            name: "SegaMail",
            website: "segamail.com",
        },
        Channel::MailSunls => ChannelInfo {
            channel: Channel::MailSunls,
            name: "Mail Sunls",
            website: "mail.sunls.de",
        },
        Channel::Expressinboxhub => ChannelInfo {
            channel: Channel::Expressinboxhub,
            name: "ExpressInboxHub",
            website: "expressinboxhub.com",
        },
        Channel::Lroid => ChannelInfo {
            channel: Channel::Lroid,
            name: "Lroid",
            website: "lroid.com",
        },
        Channel::Haribu => ChannelInfo {
            channel: Channel::Haribu,
            name: "Haribu",
            website: "haribu.net",
        },
        Channel::Pleasenospam => ChannelInfo {
            channel: Channel::Pleasenospam,
            name: "PleeaseNoSpam",
            website: "pleasenospam.email",
        },
        Channel::Rootsh => ChannelInfo {
            channel: Channel::Rootsh,
            name: "Rootsh(BccTo)",
            website: "rootsh.com",
        },
        Channel::FakeEmailSite => ChannelInfo {
            channel: Channel::FakeEmailSite,
            name: "FakeEmailSite",
            website: "fake-email.site",
        },
        Channel::Mohmal => ChannelInfo {
            channel: Channel::Mohmal,
            name: "Mohmal",
            website: "mohmal.com",
        },
        Channel::Mailgolem => ChannelInfo {
            channel: Channel::Mailgolem,
            name: "MailGolem",
            website: "mailgolem.com",
        },
        Channel::BestTempMail => ChannelInfo {
            channel: Channel::BestTempMail,
            name: "BestTempMail",
            website: "best-temp-mail.com",
        },
        Channel::DisposablemailApp => ChannelInfo {
            channel: Channel::DisposablemailApp,
            name: "DisposableMail",
            website: "disposablemail.app",
        },
        Channel::MailtempCc => ChannelInfo {
            channel: Channel::MailtempCc,
            name: "MailTemp.cc",
            website: "mailtemp.cc",
        },
        Channel::Minuteinbox => ChannelInfo {
            channel: Channel::Minuteinbox,
            name: "MinuteInbox",
            website: "minuteinbox.com",
        },
        Channel::Mailcatch => ChannelInfo {
            channel: Channel::Mailcatch,
            name: "MailCatch",
            website: "mailcatch.com",
        },
        Channel::TempemailCo => ChannelInfo {
            channel: Channel::TempemailCo,
            name: "TempEmail.co",
            website: "tempemail.co",
        },
        Channel::TempemailsNet => ChannelInfo {
            channel: Channel::TempemailsNet,
            name: "TempEmails.net",
            website: "tempemails.net",
        },
        Channel::Altmails => ChannelInfo {
            channel: Channel::Altmails,
            name: "AltMails",
            website: "tempmail.altmails.com",
        },
        Channel::TempemailInfo => ChannelInfo {
            channel: Channel::TempemailInfo,
            name: "TempEmailInfo",
            website: "tempemail.info",
        },
        Channel::Smailpro => ChannelInfo {
            channel: Channel::Smailpro,
            name: "SmailPro",
            website: "smailpro.com",
        },
        Channel::Tempmailten => ChannelInfo {
            channel: Channel::Tempmailten,
            name: "TempMailTen",
            website: "tempmailten.com",
        },
        Channel::MaildropCc => ChannelInfo {
            channel: Channel::MaildropCc,
            name: "MailDrop.cc",
            website: "maildrop.cc",
        },
        Channel::TenminutemailNet => ChannelInfo {
            channel: Channel::TenminutemailNet,
            name: "10MinuteMail.net",
            website: "10minutemail.net",
        },
        Channel::LinshiyouxiangNet => ChannelInfo {
            channel: Channel::LinshiyouxiangNet,
            name: "LinShiYouXiang",
            website: "linshiyouxiang.net",
        },
        Channel::TempmailFyi => ChannelInfo {
            channel: Channel::TempmailFyi,
            name: "TempMail.FYI",
            website: "temp-mail.fyi",
        },
        Channel::DisposablemailCom => ChannelInfo {
            channel: Channel::DisposablemailCom,
            name: "DisposableMail",
            website: "disposablemail.com",
        },
        Channel::TemppMails => ChannelInfo {
            channel: Channel::TemppMails,
            name: "TempP-Mails",
            website: "tempp-mails.com",
        },
        Channel::EmailtempOrg => ChannelInfo {
            channel: Channel::EmailtempOrg,
            name: "EmailTemp.org",
            website: "emailtemp.org",
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
/// 返回值是本端运行时随机顺序，不作为跨 SDK 一致性约束
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
        Channel::XghffCom => providers::tenminute_one::generate_email(Some("xghff.com"))
            .map(|info| with_channel(info, Channel::XghffCom)),
        Channel::OqqajCom => providers::tenminute_one::generate_email(Some("oqqaj.com"))
            .map(|info| with_channel(info, Channel::OqqajCom)),
        Channel::PsovvCom => providers::tenminute_one::generate_email(Some("psovv.com"))
            .map(|info| with_channel(info, Channel::PsovvCom)),
        Channel::DbwotCom => providers::tenminute_one::generate_email(Some("dbwot.com"))
            .map(|info| with_channel(info, Channel::DbwotCom)),
        Channel::YgwprCom => providers::tenminute_one::generate_email(Some("ygwpr.com"))
            .map(|info| with_channel(info, Channel::YgwprCom)),
        Channel::ImxweCom => providers::tenminute_one::generate_email(Some("imxwe.com"))
            .map(|info| with_channel(info, Channel::ImxweCom)),
        Channel::Linshiyou => providers::linshiyou::generate_email(),
        Channel::Mffac => providers::mffac::generate_email(),
        Channel::TempmailLol => providers::tempmail_lol::generate_email(domain),
        Channel::ChatgptOrgUk => providers::chatgpt_org_uk::generate_email(),
        Channel::TempMailIo => providers::temp_mail_io::generate_email(),
        Channel::MailCx => providers::mail_cx::generate_email(domain),
        Channel::DdkerCom => providers::mail_cx::generate_email(Some("ddker.com"))
            .map(|info| with_channel(info, Channel::DdkerCom)),
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
        Channel::Tempmailc => providers::tempmailc::generate_email(),
        Channel::Mailnesia => providers::mailnesia::generate_email(),
        Channel::Throwawaymail => providers::throwawaymail::generate_email(),
        Channel::ShittyEmail => providers::shitty_email::generate_email(),
        Channel::Tempmailpro => providers::tempmailpro::generate_email(),
        Channel::DevmailUk => providers::devmail_uk::generate_email(),
        Channel::CleanTempMail => providers::cleantempmail::generate_email(),
        Channel::Inboxkitten => providers::inboxkitten::generate_email(),
        Channel::Getnada => providers::getnada::generate_email(domain),
        Channel::OneVpnNet => providers::getnada::generate_email(Some("1vpn.net"))
            .map(|info| with_channel(info, Channel::OneVpnNet)),
        Channel::AbematvCom => providers::getnada::generate_email(Some("abematv.com"))
            .map(|info| with_channel(info, Channel::AbematvCom)),
        Channel::AbematvNet => providers::getnada::generate_email(Some("abematv.net"))
            .map(|info| with_channel(info, Channel::AbematvNet)),
        Channel::AbematvOrg => providers::getnada::generate_email(Some("abematv.org"))
            .map(|info| with_channel(info, Channel::AbematvOrg)),
        Channel::AcehCc => providers::getnada::generate_email(Some("aceh.cc"))
            .map(|info| with_channel(info, Channel::AcehCc)),
        Channel::BangkabelitungNet => {
            providers::getnada::generate_email(Some("bangkabelitung.net"))
                .map(|info| with_channel(info, Channel::BangkabelitungNet))
        }
        Channel::CctruyenCom => providers::getnada::generate_email(Some("cctruyen.com"))
            .map(|info| with_channel(info, Channel::CctruyenCom)),
        Channel::GetnadaCom => providers::getnada::generate_email(Some("getnada.com"))
            .map(|info| with_channel(info, Channel::GetnadaCom)),
        Channel::GetnadaEmail => providers::getnada::generate_email(Some("getnada.email"))
            .map(|info| with_channel(info, Channel::GetnadaEmail)),
        Channel::GetnadaNet => providers::getnada::generate_email(Some("getnada.net"))
            .map(|info| with_channel(info, Channel::GetnadaNet)),
        Channel::JawatengahNet => providers::getnada::generate_email(Some("jawatengah.net"))
            .map(|info| with_channel(info, Channel::JawatengahNet)),
        Channel::JawatimurNet => providers::getnada::generate_email(Some("jawatimur.net"))
            .map(|info| with_channel(info, Channel::JawatimurNet)),
        Channel::KalimantanbaratNet => {
            providers::getnada::generate_email(Some("kalimantanbarat.net"))
                .map(|info| with_channel(info, Channel::KalimantanbaratNet))
        }
        Channel::KalimantanselatanNet => {
            providers::getnada::generate_email(Some("kalimantanselatan.net"))
                .map(|info| with_channel(info, Channel::KalimantanselatanNet))
        }
        Channel::KalimantantengahNet => {
            providers::getnada::generate_email(Some("kalimantantengah.net"))
                .map(|info| with_channel(info, Channel::KalimantantengahNet))
        }
        Channel::KalimantantimurNet => {
            providers::getnada::generate_email(Some("kalimantantimur.net"))
                .map(|info| with_channel(info, Channel::KalimantantimurNet))
        }
        Channel::KalimantanutaraNet => {
            providers::getnada::generate_email(Some("kalimantanutara.net"))
                .map(|info| with_channel(info, Channel::KalimantanutaraNet))
        }
        Channel::KepulauanriauNet => providers::getnada::generate_email(Some("kepulauanriau.net"))
            .map(|info| with_channel(info, Channel::KepulauanriauNet)),
        Channel::Luxury345Com => providers::getnada::generate_email(Some("luxury345.com"))
            .map(|info| with_channel(info, Channel::Luxury345Com)),
        Channel::MalukuutaraNet => providers::getnada::generate_email(Some("malukuutara.net"))
            .map(|info| with_channel(info, Channel::MalukuutaraNet)),
        Channel::NusatenggarabaratNet => {
            providers::getnada::generate_email(Some("nusatenggarabarat.net"))
                .map(|info| with_channel(info, Channel::NusatenggarabaratNet))
        }
        Channel::NusatenggaratimurNet => {
            providers::getnada::generate_email(Some("nusatenggaratimur.net"))
                .map(|info| with_channel(info, Channel::NusatenggaratimurNet))
        }
        Channel::PapuabaratNet => providers::getnada::generate_email(Some("papuabarat.net"))
            .map(|info| with_channel(info, Channel::PapuabaratNet)),
        Channel::PapuabaratdayaNet => {
            providers::getnada::generate_email(Some("papuabaratdaya.net"))
                .map(|info| with_channel(info, Channel::PapuabaratdayaNet))
        }
        Channel::PapuaselatanNet => providers::getnada::generate_email(Some("papuaselatan.net"))
            .map(|info| with_channel(info, Channel::PapuaselatanNet)),
        Channel::PeholCom => providers::getnada::generate_email(Some("pehol.com"))
            .map(|info| with_channel(info, Channel::PeholCom)),
        Channel::PtruyenCom => providers::getnada::generate_email(Some("ptruyen.com"))
            .map(|info| with_channel(info, Channel::PtruyenCom)),
        Channel::PulaubaliNet => providers::getnada::generate_email(Some("pulaubali.net"))
            .map(|info| with_channel(info, Channel::PulaubaliNet)),
        Channel::RiauNet => providers::getnada::generate_email(Some("riau.net"))
            .map(|info| with_channel(info, Channel::RiauNet)),
        Channel::SeokeyOrg => providers::getnada::generate_email(Some("seokey.org"))
            .map(|info| with_channel(info, Channel::SeokeyOrg)),
        Channel::SulawesibaratNet => providers::getnada::generate_email(Some("sulawesibarat.net"))
            .map(|info| with_channel(info, Channel::SulawesibaratNet)),
        Channel::SulawesiselatanNet => {
            providers::getnada::generate_email(Some("sulawesiselatan.net"))
                .map(|info| with_channel(info, Channel::SulawesiselatanNet))
        }
        Channel::SulawesitengahNet => {
            providers::getnada::generate_email(Some("sulawesitengah.net"))
                .map(|info| with_channel(info, Channel::SulawesitengahNet))
        }
        Channel::SulawesitenggaraNet => {
            providers::getnada::generate_email(Some("sulawesitenggara.net"))
                .map(|info| with_channel(info, Channel::SulawesitenggaraNet))
        }
        Channel::SumaterabaratNet => providers::getnada::generate_email(Some("sumaterabarat.net"))
            .map(|info| with_channel(info, Channel::SumaterabaratNet)),
        Channel::SumateraselatanNet => {
            providers::getnada::generate_email(Some("sumateraselatan.net"))
                .map(|info| with_channel(info, Channel::SumateraselatanNet))
        }
        Channel::SumaterautaraNet => providers::getnada::generate_email(Some("sumaterautara.net"))
            .map(|info| with_channel(info, Channel::SumaterautaraNet)),
        Channel::VillatogelCom => providers::getnada::generate_email(Some("villatogel.com"))
            .map(|info| with_channel(info, Channel::VillatogelCom)),
        Channel::Mail123 => providers::mail123::generate_email(),
        Channel::Mail10s => providers::mail10s::generate_email(),
        Channel::Webmailtemp => providers::webmailtemp::generate_email(),
        Channel::Tempfastmail => providers::tempfastmail::generate_email(),
        Channel::OneSecMail => providers::one_sec_mail::generate_email(),
        Channel::Fakemail => providers::fakemail::generate_email(),
        Channel::Openinbox => providers::openinbox::generate_email(),
        Channel::Inboxes => providers::inboxes::generate_email(domain),
        Channel::Uncorreotemporal => providers::uncorreotemporal::generate_email(),
        Channel::Awamail => providers::awamail::generate_email(),
        Channel::MailTm => providers::mail_tm::generate_email(),
        Channel::WebLibraryNet => providers::mail_tm::generate_email()
            .map(|info| with_channel(info, Channel::WebLibraryNet)),
        Channel::Dropmail => providers::dropmail::generate_email(),
        Channel::GuerrillaMail => providers::guerrillamail::generate_email(),
        Channel::GuerrillamailCom => {
            providers::guerrillamail_mirrors::guerrillamail_com::generate_email()
        }
        Channel::Maildrop => providers::maildrop::generate_email(domain),
        Channel::SmailPw => providers::smail_pw::generate_email(),
        Channel::Vip215 => providers::vip_215::generate_email(),
        Channel::FakeLegal => providers::fake_legal::generate_email(domain),
        Channel::ImguiDe => providers::fake_legal::generate_email(Some("imgui.de"))
            .map(|info| with_channel(info, Channel::ImguiDe)),
        Channel::PulsewebmenuDe => providers::fake_legal::generate_email(Some("pulsewebmenu.de"))
            .map(|info| with_channel(info, Channel::PulsewebmenuDe)),
        Channel::Moakt => providers::moakt::generate_email(domain),
        Channel::DrmailIn => providers::moakt::generate_email(Some("drmail.in"))
            .map(|info| with_channel(info, Channel::DrmailIn)),
        Channel::TemlNet => providers::moakt::generate_email(Some("teml.net"))
            .map(|info| with_channel(info, Channel::TemlNet)),
        Channel::TmpemlCom => providers::moakt::generate_email(Some("tmpeml.com"))
            .map(|info| with_channel(info, Channel::TmpemlCom)),
        Channel::TmpboxNet => providers::moakt::generate_email(Some("tmpbox.net"))
            .map(|info| with_channel(info, Channel::TmpboxNet)),
        Channel::MoaktCc => providers::moakt::generate_email(Some("moakt.cc"))
            .map(|info| with_channel(info, Channel::MoaktCc)),
        Channel::DisboxNet => providers::moakt::generate_email(Some("disbox.net"))
            .map(|info| with_channel(info, Channel::DisboxNet)),
        Channel::TmpmailOrg => providers::moakt::generate_email(Some("tmpmail.org"))
            .map(|info| with_channel(info, Channel::TmpmailOrg)),
        Channel::TmpmailNet => providers::moakt::generate_email(Some("tmpmail.net"))
            .map(|info| with_channel(info, Channel::TmpmailNet)),
        Channel::TmailsNet => providers::moakt::generate_email(Some("tmails.net"))
            .map(|info| with_channel(info, Channel::TmailsNet)),
        Channel::DisboxOrg => providers::moakt::generate_email(Some("disbox.org"))
            .map(|info| with_channel(info, Channel::DisboxOrg)),
        Channel::MoaktCo => providers::moakt::generate_email(Some("moakt.co"))
            .map(|info| with_channel(info, Channel::MoaktCo)),
        Channel::MoaktWs => providers::moakt::generate_email(Some("moakt.ws"))
            .map(|info| with_channel(info, Channel::MoaktWs)),
        Channel::TmailWs => providers::moakt::generate_email(Some("tmail.ws"))
            .map(|info| with_channel(info, Channel::TmailWs)),
        Channel::BareedWs => providers::moakt::generate_email(Some("bareed.ws"))
            .map(|info| with_channel(info, Channel::BareedWs)),
        Channel::Email10min => providers::email10min::generate_email(),
        Channel::MjjCm => providers::socketio_mail::mjj_cm::generate_email(),
        Channel::LinshiCo => providers::socketio_mail::linshi_co::generate_email(),
        Channel::Harakirimail => providers::harakirimail::generate_email(),
        Channel::TempmailPlus => {
            providers::tempmail_plus::generate_email(None, Channel::TempmailPlus)
        }
        Channel::FexpostCom => {
            providers::tempmail_plus::generate_email(Some("fexpost.com"), Channel::FexpostCom)
        }
        Channel::FexboxOrg => {
            providers::tempmail_plus::generate_email(Some("fexbox.org"), Channel::FexboxOrg)
        }
        Channel::MailboxInUa => {
            providers::tempmail_plus::generate_email(Some("mailbox.in.ua"), Channel::MailboxInUa)
        }
        Channel::RoverInfo => {
            providers::tempmail_plus::generate_email(Some("rover.info"), Channel::RoverInfo)
        }
        Channel::ChitthiIn => {
            providers::tempmail_plus::generate_email(Some("chitthi.in"), Channel::ChitthiIn)
        }
        Channel::FextempCom => {
            providers::tempmail_plus::generate_email(Some("fextemp.com"), Channel::FextempCom)
        }
        Channel::AnyPink => {
            providers::tempmail_plus::generate_email(Some("any.pink"), Channel::AnyPink)
        }
        Channel::MerepostCom => {
            providers::tempmail_plus::generate_email(Some("merepost.com"), Channel::MerepostCom)
        }
        Channel::TempmailLolV2 => providers::tempmail_lol_v2::generate_email(),
        Channel::Tempgbox => providers::tempgbox::generate_email(),
        Channel::Emailnator => providers::emailnator::generate_email(),
        Channel::Temporam => providers::temporam::generate_email(domain),
        Channel::Neighbours => providers::neighbours::generate_email(domain),
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
        Channel::JqkjqkXyz => providers::zhujump::generate_email("jqkjqk.xyz", Channel::JqkjqkXyz),
        Channel::LyhleviCom => providers::zhujump::generate_email_for_instance(
            "https://lyhlevi.com",
            "lyhlevi.com",
            Channel::LyhleviCom,
            Some(24 * 60 * 60 * 1000),
        ),
        Channel::M2u => providers::m2u::generate_email(),
        Channel::TempyEmail => providers::tempy_email::generate_email(domain),
        Channel::Fmail => providers::fmail::generate_email(domain),
        Channel::Ockito => providers::ockito::generate_email(),
        Channel::Anonbox => providers::anonbox::generate_email(),
        Channel::Mailinator => providers::mailinator::generate_email(),
        Channel::Duckmail => providers::duckmail::generate_email(),
        Channel::Tempmail365 => providers::tempmail365::generate_email(domain),
        Channel::Tempinbox => providers::tempinbox::generate_email(domain),
        Channel::Byom => providers::byom::generate_email(),
        Channel::Anonymmail => providers::anonymmail::generate_email(),
        Channel::Eyepaste => providers::eyepaste::generate_email(),
        Channel::Segamail => providers::segamail::generate_email(),
        Channel::MailSunls => providers::mail_sunls::generate_email(),
        Channel::Expressinboxhub => providers::expressinboxhub::generate_email(),
        Channel::Lroid => providers::lroid::generate_email(),
        Channel::Haribu => providers::haribu::generate_email(),
        Channel::Pleasenospam => providers::pleasenospam::generate_email(),
        Channel::Rootsh => providers::rootsh::generate_email(),
        Channel::FakeEmailSite => providers::fake_email_site::generate_email(),
        Channel::Mohmal => providers::mohmal::generate_email(),
        Channel::Mailgolem => providers::mailgolem::generate_email(),
        Channel::BestTempMail => providers::best_temp_mail::generate_email(),
        Channel::DisposablemailApp => providers::disposablemail_app::generate_email(),
        Channel::MailtempCc => providers::mailtemp_cc::generate_email(),
        Channel::Minuteinbox => providers::minuteinbox::generate_email(),
        Channel::Mailcatch => providers::minuteinbox::generate_email()
            .map(|info| with_channel(info, Channel::Mailcatch)),
        Channel::TempemailCo => providers::tempemail_co::generate_email(),
        Channel::TempemailsNet => providers::tempemails_net::generate_email(),
        Channel::Altmails => providers::altmails::generate_email(),
        Channel::TempemailInfo => providers::tempemail_info::generate_email(),
        Channel::Smailpro => providers::smailpro::generate_email(),
        Channel::Tempmailten => providers::tempmailten::generate_email(),
        Channel::MaildropCc => providers::maildrop_cc::generate_email(),
        Channel::TenminutemailNet => providers::tenminutemail_net::generate_email(),
        Channel::LinshiyouxiangNet => providers::linshiyouxiang_net::generate_email(),
        Channel::TempmailFyi => providers::tempmail_fyi::generate_email(),
        Channel::DisposablemailCom => providers::disposablemail_com::generate_email(),
        Channel::TemppMails => providers::tempp_mails::generate_email(),
        Channel::EmailtempOrg => providers::emailtemp_org::generate_email(),
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
        Channel::TenminuteOne
        | Channel::XghffCom
        | Channel::OqqajCom
        | Channel::PsovvCom
        | Channel::DbwotCom
        | Channel::YgwprCom
        | Channel::ImxweCom => {
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
        Channel::MailCx | Channel::DdkerCom => {
            let t = token.ok_or("token is required for mail-cx")?;
            providers::mail_cx::get_emails(t, email)
        }
        Channel::Catchmail => providers::catchmail::get_emails(email),
        Channel::CatchmailMailistry => providers::catchmail::get_emails(email),
        Channel::CatchmailZeppost => providers::catchmail::get_emails(email),
        Channel::Mailforspam => providers::mailforspam::get_emails(email),
        Channel::MailforspamTempmailIo => providers::mailforspam::get_emails(email),
        Channel::MailforspamDisposable => providers::mailforspam::get_emails(email),
        Channel::Tempmailc => providers::tempmailc::get_emails(email),
        Channel::Mailnesia => providers::mailnesia::get_emails(email),
        Channel::Throwawaymail => {
            let t = token.ok_or("token is required for throwawaymail")?;
            providers::throwawaymail::get_emails(t, email)
        }
        Channel::ShittyEmail => {
            let t = token.ok_or("token is required for shitty-email")?;
            providers::shitty_email::get_emails(t, email)
        }
        Channel::Tempmailpro => {
            let t = token.ok_or("token is required for tempmailpro")?;
            providers::tempmailpro::get_emails(t, email)
        }
        Channel::DevmailUk => providers::devmail_uk::get_emails(email),
        Channel::CleanTempMail => providers::cleantempmail::get_emails(email),
        Channel::Inboxkitten => providers::inboxkitten::get_emails(email),
        Channel::Getnada
        | Channel::OneVpnNet
        | Channel::AbematvCom
        | Channel::AbematvNet
        | Channel::AbematvOrg
        | Channel::AcehCc
        | Channel::BangkabelitungNet
        | Channel::CctruyenCom
        | Channel::GetnadaCom
        | Channel::GetnadaEmail
        | Channel::GetnadaNet
        | Channel::JawatengahNet
        | Channel::JawatimurNet
        | Channel::KalimantanbaratNet
        | Channel::KalimantanselatanNet
        | Channel::KalimantantengahNet
        | Channel::KalimantantimurNet
        | Channel::KalimantanutaraNet
        | Channel::KepulauanriauNet
        | Channel::Luxury345Com
        | Channel::MalukuutaraNet
        | Channel::NusatenggarabaratNet
        | Channel::NusatenggaratimurNet
        | Channel::PapuabaratNet
        | Channel::PapuabaratdayaNet
        | Channel::PapuaselatanNet
        | Channel::PeholCom
        | Channel::PtruyenCom
        | Channel::PulaubaliNet
        | Channel::RiauNet
        | Channel::SeokeyOrg
        | Channel::SulawesibaratNet
        | Channel::SulawesiselatanNet
        | Channel::SulawesitengahNet
        | Channel::SulawesitenggaraNet
        | Channel::SumaterabaratNet
        | Channel::SumateraselatanNet
        | Channel::SumaterautaraNet
        | Channel::VillatogelCom => {
            let t = token.ok_or("token is required for getnada")?;
            providers::getnada::get_emails(t, email)
        }
        Channel::Mail123 => providers::mail123::get_emails(email),
        Channel::Mail10s => providers::mail10s::get_emails(email),
        Channel::Webmailtemp => {
            let t = token.ok_or("token is required for webmailtemp")?;
            providers::webmailtemp::get_emails(t, email)
        }
        Channel::Tempfastmail => {
            let t = token.ok_or("token is required for tempfastmail")?;
            providers::tempfastmail::get_emails(t, email)
        }
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
        Channel::MailTm | Channel::WebLibraryNet => {
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
        Channel::FakeLegal | Channel::ImguiDe | Channel::PulsewebmenuDe => {
            providers::fake_legal::get_emails(email)
        }
        Channel::Moakt
        | Channel::DrmailIn
        | Channel::TemlNet
        | Channel::TmpemlCom
        | Channel::TmpboxNet
        | Channel::MoaktCc
        | Channel::DisboxNet
        | Channel::TmpmailOrg
        | Channel::TmpmailNet
        | Channel::TmailsNet
        | Channel::DisboxOrg
        | Channel::MoaktCo
        | Channel::MoaktWs
        | Channel::TmailWs
        | Channel::BareedWs => {
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
        Channel::TempmailPlus
        | Channel::FexpostCom
        | Channel::FexboxOrg
        | Channel::MailboxInUa
        | Channel::RoverInfo
        | Channel::ChitthiIn
        | Channel::FextempCom
        | Channel::AnyPink
        | Channel::MerepostCom => providers::tempmail_plus::get_emails(email),
        Channel::TempmailLolV2 => {
            let t = token.ok_or("token is required for tempmail-lol-v2")?;
            providers::tempmail_lol_v2::get_emails(t, email)
        }
        Channel::Tempgbox => {
            let t = token.ok_or("token is required for tempgbox")?;
            providers::tempgbox::get_emails(t, email)
        }
        Channel::Emailnator => {
            let t = token.ok_or("token is required for emailnator")?;
            providers::emailnator::get_emails(t, email)
        }
        Channel::Temporam => providers::temporam::get_emails(token, email),
        Channel::Neighbours => providers::neighbours::get_emails(email),
        Channel::Fmail => providers::fmail::get_emails(email),
        Channel::Ockito => {
            let t = token.ok_or("token is required for ockito")?;
            providers::ockito::get_emails(t, email)
        }
        Channel::Anonbox => {
            let t = token.ok_or("token is required for anonbox")?;
            providers::anonbox::get_emails(t, email)
        }
        Channel::Mailinator => providers::mailinator::get_emails(email),
        Channel::Duckmail => {
            let t = token.ok_or("token is required for duckmail")?;
            providers::duckmail::get_emails(t, email)
        }
        Channel::Tempmail365 => providers::tempmail365::get_emails(email),
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
        Channel::JqkjqkXyz | Channel::LyhleviCom => {
            let t = token.ok_or("token is required for moemail")?;
            providers::zhujump::get_emails(t, email)
        }
        Channel::M2u => {
            let t = token.ok_or("token is required for m2u")?;
            providers::m2u::get_emails(t, email)
        }
        Channel::TempyEmail => providers::tempy_email::get_emails(email),
        Channel::Tempinbox => providers::tempinbox::get_emails(email),
        Channel::Byom => providers::byom::get_emails(email),
        Channel::Anonymmail => providers::anonymmail::get_emails(email),
        Channel::Eyepaste => providers::eyepaste::get_emails(email),
        Channel::Segamail => {
            let t = token.ok_or("token is required for segamail")?;
            providers::segamail::get_emails(t, email)
        }
        Channel::MailSunls => providers::mail_sunls::get_emails(email),
        Channel::Expressinboxhub => {
            let t = token.ok_or("token is required for expressinboxhub")?;
            providers::expressinboxhub::get_emails(t, email)
        }
        Channel::Lroid => {
            let t = token.ok_or("token is required for lroid")?;
            providers::lroid::get_emails(t, email)
        }
        Channel::Haribu => {
            let t = token.ok_or("token is required for haribu")?;
            providers::haribu::get_emails(t, email)
        }
        Channel::Pleasenospam => providers::pleasenospam::get_emails(email),
        Channel::Rootsh => {
            let t = token.ok_or("token is required for rootsh")?;
            providers::rootsh::get_emails(t, email)
        }
        Channel::FakeEmailSite => providers::fake_email_site::get_emails(email),
        Channel::Mohmal => {
            let t = token.ok_or("token is required for mohmal")?;
            providers::mohmal::get_emails(t, email)
        }
        Channel::Mailgolem => {
            let t = token.ok_or("token is required for mailgolem")?;
            providers::mailgolem::get_emails(t, email)
        }
        Channel::BestTempMail => {
            let t = token.ok_or("token is required for best-temp-mail")?;
            providers::best_temp_mail::get_emails(t, email)
        }
        Channel::DisposablemailApp => {
            let t = token.ok_or("token is required for disposablemail-app")?;
            providers::disposablemail_app::get_emails(t, email)
        }
        Channel::MailtempCc => {
            let t = token.ok_or("token is required for mailtemp-cc")?;
            providers::mailtemp_cc::get_emails(t, email)
        }
        Channel::Minuteinbox | Channel::Mailcatch => {
            let t = token.ok_or("token is required for minuteinbox")?;
            providers::minuteinbox::get_emails(t, email)
        }
        Channel::TempemailCo => {
            let t = token.ok_or("token is required for tempemail-co")?;
            providers::tempemail_co::get_emails(t, email)
        }
        Channel::TempemailsNet => {
            let t = token.ok_or("token is required for tempemails-net")?;
            providers::tempemails_net::get_emails(t, email)
        }
        Channel::Altmails => {
            let t = token.ok_or("token is required for altmails")?;
            providers::altmails::get_emails(t, email)
        }
        Channel::TempemailInfo => {
            let t = token.ok_or("token is required for tempemail-info")?;
            providers::tempemail_info::get_emails(t, email)
        }
        Channel::Smailpro => {
            let t = token.ok_or("token is required for smailpro")?;
            providers::smailpro::get_emails(t, email)
        }
        Channel::Tempmailten => {
            let t = token.ok_or("token is required for tempmailten")?;
            providers::tempmailten::get_emails(t, email)
        }
        Channel::MaildropCc => {
            let t = token.ok_or("token is required for maildrop-cc")?;
            providers::maildrop_cc::get_emails(t, email)
        }
        Channel::TenminutemailNet => {
            let t = token.ok_or("token is required for 10minutemail-net")?;
            providers::tenminutemail_net::get_emails(t, email)
        }
        Channel::LinshiyouxiangNet => {
            let t = token.ok_or("token is required for linshiyouxiang-net")?;
            providers::linshiyouxiang_net::get_emails(t, email)
        }
        Channel::TempmailFyi => {
            let t = token.ok_or("token is required for tempmail-fyi")?;
            providers::tempmail_fyi::get_emails(t, email)
        }
        Channel::DisposablemailCom => {
            let t = token.ok_or("token is required for disposablemail-com")?;
            providers::disposablemail_com::get_emails(t, email)
        }
        Channel::TemppMails => {
            let t = token.ok_or("token is required for tempp-mails")?;
            providers::tempp_mails::get_emails(t, email)
        }
        Channel::EmailtempOrg => {
            let t = token.ok_or("token is required for emailtemp-org")?;
            providers::emailtemp_org::get_emails(t, email)
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
