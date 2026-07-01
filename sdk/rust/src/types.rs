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
    #[serde(rename = "xghff-com")]
    XghffCom,
    #[serde(rename = "oqqaj-com")]
    OqqajCom,
    #[serde(rename = "psovv-com")]
    PsovvCom,
    #[serde(rename = "dbwot-com")]
    DbwotCom,
    #[serde(rename = "ygwpr-com")]
    YgwprCom,
    #[serde(rename = "imxwe-com")]
    ImxweCom,
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
    #[serde(rename = "ddker-com")]
    DdkerCom,
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
    #[serde(rename = "tempmailc")]
    Tempmailc,
    #[serde(rename = "mailnesia")]
    Mailnesia,
    #[serde(rename = "throwawaymail")]
    Throwawaymail,
    #[serde(rename = "shitty-email")]
    ShittyEmail,
    #[serde(rename = "tempmailpro")]
    Tempmailpro,
    #[serde(rename = "devmail-uk")]
    DevmailUk,
    #[serde(rename = "cleantempmail")]
    CleanTempMail,
    #[serde(rename = "inboxkitten")]
    Inboxkitten,
    #[serde(rename = "getnada")]
    Getnada,
    #[serde(rename = "1vpn-net")]
    OneVpnNet,
    #[serde(rename = "abematv-com")]
    AbematvCom,
    #[serde(rename = "abematv-net")]
    AbematvNet,
    #[serde(rename = "abematv-org")]
    AbematvOrg,
    #[serde(rename = "aceh-cc")]
    AcehCc,
    #[serde(rename = "bangkabelitung-net")]
    BangkabelitungNet,
    #[serde(rename = "cctruyen-com")]
    CctruyenCom,
    #[serde(rename = "getnada-com")]
    GetnadaCom,
    #[serde(rename = "getnada-email")]
    GetnadaEmail,
    #[serde(rename = "getnada-net")]
    GetnadaNet,
    #[serde(rename = "jawatengah-net")]
    JawatengahNet,
    #[serde(rename = "jawatimur-net")]
    JawatimurNet,
    #[serde(rename = "kalimantanbarat-net")]
    KalimantanbaratNet,
    #[serde(rename = "kalimantanselatan-net")]
    KalimantanselatanNet,
    #[serde(rename = "kalimantantengah-net")]
    KalimantantengahNet,
    #[serde(rename = "kalimantantimur-net")]
    KalimantantimurNet,
    #[serde(rename = "kalimantanutara-net")]
    KalimantanutaraNet,
    #[serde(rename = "kepulauanriau-net")]
    KepulauanriauNet,
    #[serde(rename = "luxury345-com")]
    Luxury345Com,
    #[serde(rename = "malukuutara-net")]
    MalukuutaraNet,
    #[serde(rename = "nusatenggarabarat-net")]
    NusatenggarabaratNet,
    #[serde(rename = "nusatenggaratimur-net")]
    NusatenggaratimurNet,
    #[serde(rename = "papuabarat-net")]
    PapuabaratNet,
    #[serde(rename = "papuabaratdaya-net")]
    PapuabaratdayaNet,
    #[serde(rename = "papuaselatan-net")]
    PapuaselatanNet,
    #[serde(rename = "pehol-com")]
    PeholCom,
    #[serde(rename = "ptruyen-com")]
    PtruyenCom,
    #[serde(rename = "pulaubali-net")]
    PulaubaliNet,
    #[serde(rename = "riau-net")]
    RiauNet,
    #[serde(rename = "seokey-org")]
    SeokeyOrg,
    #[serde(rename = "sulawesibarat-net")]
    SulawesibaratNet,
    #[serde(rename = "sulawesiselatan-net")]
    SulawesiselatanNet,
    #[serde(rename = "sulawesitengah-net")]
    SulawesitengahNet,
    #[serde(rename = "sulawesitenggara-net")]
    SulawesitenggaraNet,
    #[serde(rename = "sumaterabarat-net")]
    SumaterabaratNet,
    #[serde(rename = "sumateraselatan-net")]
    SumateraselatanNet,
    #[serde(rename = "sumaterautara-net")]
    SumaterautaraNet,
    #[serde(rename = "villatogel-com")]
    VillatogelCom,
    #[serde(rename = "mail123")]
    Mail123,
    #[serde(rename = "mail10s")]
    Mail10s,
    #[serde(rename = "webmailtemp")]
    Webmailtemp,
    #[serde(rename = "tempfastmail")]
    Tempfastmail,
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
    #[serde(rename = "web-library-net")]
    WebLibraryNet,
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
    #[serde(rename = "imgui-de")]
    ImguiDe,
    #[serde(rename = "pulsewebmenu-de")]
    PulsewebmenuDe,
    #[serde(rename = "moakt")]
    Moakt,
    #[serde(rename = "drmail-in")]
    DrmailIn,
    #[serde(rename = "teml-net")]
    TemlNet,
    #[serde(rename = "tmpeml-com")]
    TmpemlCom,
    #[serde(rename = "tmpbox-net")]
    TmpboxNet,
    #[serde(rename = "moakt-cc")]
    MoaktCc,
    #[serde(rename = "disbox-net")]
    DisboxNet,
    #[serde(rename = "tmpmail-org")]
    TmpmailOrg,
    #[serde(rename = "tmpmail-net")]
    TmpmailNet,
    #[serde(rename = "tmails-net")]
    TmailsNet,
    #[serde(rename = "disbox-org")]
    DisboxOrg,
    #[serde(rename = "moakt-co")]
    MoaktCo,
    #[serde(rename = "moakt-ws")]
    MoaktWs,
    #[serde(rename = "tmail-ws")]
    TmailWs,
    #[serde(rename = "bareed-ws")]
    BareedWs,
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
    #[serde(rename = "fexpost-com")]
    FexpostCom,
    #[serde(rename = "fexbox-org")]
    FexboxOrg,
    #[serde(rename = "mailbox-in-ua")]
    MailboxInUa,
    #[serde(rename = "rover-info")]
    RoverInfo,
    #[serde(rename = "chitthi-in")]
    ChitthiIn,
    #[serde(rename = "fextemp-com")]
    FextempCom,
    #[serde(rename = "any-pink")]
    AnyPink,
    #[serde(rename = "merepost-com")]
    MerepostCom,
    #[serde(rename = "tempmail-lol-v2")]
    TempmailLolV2,
    #[serde(rename = "tempgbox")]
    Tempgbox,
    #[serde(rename = "emailnator")]
    Emailnator,
    #[serde(rename = "temporam")]
    Temporam,
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
    #[serde(rename = "jqkjqk-xyz")]
    JqkjqkXyz,
    #[serde(rename = "lyhlevi-com")]
    LyhleviCom,
    #[serde(rename = "neighbours")]
    Neighbours,
    #[serde(rename = "m2u")]
    M2u,
    #[serde(rename = "tempy-email")]
    TempyEmail,
    #[serde(rename = "fmail")]
    Fmail,
    #[serde(rename = "ockito")]
    Ockito,
    #[serde(rename = "anonbox")]
    Anonbox,
    #[serde(rename = "mailinator")]
    Mailinator,
    #[serde(rename = "duckmail")]
    Duckmail,
    #[serde(rename = "tempmail365")]
    Tempmail365,
    #[serde(rename = "tempinbox")]
    Tempinbox,
    #[serde(rename = "byom")]
    Byom,
    #[serde(rename = "anonymmail")]
    Anonymmail,
    #[serde(rename = "eyepaste")]
    Eyepaste,
    #[serde(rename = "segamail")]
    Segamail,
    #[serde(rename = "mail-sunls")]
    MailSunls,
    #[serde(rename = "expressinboxhub")]
    Expressinboxhub,
    #[serde(rename = "lroid")]
    Lroid,
    #[serde(rename = "haribu")]
    Haribu,
    #[serde(rename = "pleasenospam")]
    Pleasenospam,
    #[serde(rename = "rootsh")]
    Rootsh,
    #[serde(rename = "fake-email-site")]
    FakeEmailSite,
    #[serde(rename = "mohmal")]
    Mohmal,
    /// mailgolem.com 渠道
    #[serde(rename = "mailgolem")]
    Mailgolem,
    /// best-temp-mail.com 渠道
    #[serde(rename = "best-temp-mail")]
    BestTempMail,
    /// disposablemail.app 渠道
    #[serde(rename = "disposablemail-app")]
    DisposablemailApp,
    /// mailtemp.cc 渠道
    #[serde(rename = "mailtemp-cc")]
    MailtempCc,
    /// minuteinbox.com 渠道
    #[serde(rename = "minuteinbox")]
    Minuteinbox,
    /// mailcatch.com 渠道
    #[serde(rename = "mailcatch")]
    Mailcatch,
    /// tempemail.co 渠道
    #[serde(rename = "tempemail-co")]
    TempemailCo,
    /// tempemails.net 渠道
    #[serde(rename = "tempemails-net")]
    TempemailsNet,
    /// altmails 渠道
    #[serde(rename = "altmails")]
    Altmails,
    /// tempemail-info 渠道
    #[serde(rename = "tempemail-info")]
    TempemailInfo,
    /// smailpro 渠道
    #[serde(rename = "smailpro")]
    Smailpro,
    /// tempmailten 渠道
    #[serde(rename = "tempmailten")]
    Tempmailten,
    /// maildrop-cc 渠道
    #[serde(rename = "maildrop-cc")]
    MaildropCc,
    /// 10minutemail-net 渠道
    #[serde(rename = "10minutemail-net")]
    TenminutemailNet,
    /// linshiyouxiang-net 渠道
    #[serde(rename = "linshiyouxiang-net")]
    LinshiyouxiangNet,
    /// tempmail-fyi 渠道
    #[serde(rename = "tempmail-fyi")]
    TempmailFyi,
    /// disposablemail-com 渠道
    #[serde(rename = "disposablemail-com")]
    DisposablemailCom,
    /// tempp-mails 渠道
    #[serde(rename = "tempp-mails")]
    TemppMails,
    /// emailtemp-org 渠道
    #[serde(rename = "emailtemp-org")]
    EmailtempOrg,
}

impl std::fmt::Display for Channel {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Channel::Tempmail => write!(f, "tempmail"),
            Channel::TempmailCn => write!(f, "tempmail-cn"),
            Channel::TaEasy => write!(f, "ta-easy"),
            Channel::TenminuteOne => write!(f, "10minute-one"),
            Channel::XghffCom => write!(f, "xghff-com"),
            Channel::OqqajCom => write!(f, "oqqaj-com"),
            Channel::PsovvCom => write!(f, "psovv-com"),
            Channel::DbwotCom => write!(f, "dbwot-com"),
            Channel::YgwprCom => write!(f, "ygwpr-com"),
            Channel::ImxweCom => write!(f, "imxwe-com"),
            Channel::Linshiyou => write!(f, "linshiyou"),
            Channel::Mffac => write!(f, "mffac"),
            Channel::TempmailLol => write!(f, "tempmail-lol"),
            Channel::ChatgptOrgUk => write!(f, "chatgpt-org-uk"),
            Channel::TempMailIo => write!(f, "temp-mail-io"),
            Channel::MailCx => write!(f, "mail-cx"),
            Channel::DdkerCom => write!(f, "ddker-com"),
            Channel::Catchmail => write!(f, "catchmail"),
            Channel::CatchmailMailistry => write!(f, "catchmail-mailistry"),
            Channel::CatchmailZeppost => write!(f, "catchmail-zeppost"),
            Channel::Mailforspam => write!(f, "mailforspam"),
            Channel::MailforspamTempmailIo => write!(f, "mailforspam-tempmail-io"),
            Channel::MailforspamDisposable => write!(f, "mailforspam-disposable"),
            Channel::Tempmailc => write!(f, "tempmailc"),
            Channel::Mailnesia => write!(f, "mailnesia"),
            Channel::Throwawaymail => write!(f, "throwawaymail"),
            Channel::ShittyEmail => write!(f, "shitty-email"),
            Channel::Tempmailpro => write!(f, "tempmailpro"),
            Channel::DevmailUk => write!(f, "devmail-uk"),
            Channel::CleanTempMail => write!(f, "cleantempmail"),
            Channel::Inboxkitten => write!(f, "inboxkitten"),
            Channel::Getnada => write!(f, "getnada"),
            Channel::OneVpnNet => write!(f, "1vpn-net"),
            Channel::AbematvCom => write!(f, "abematv-com"),
            Channel::AbematvNet => write!(f, "abematv-net"),
            Channel::AbematvOrg => write!(f, "abematv-org"),
            Channel::AcehCc => write!(f, "aceh-cc"),
            Channel::BangkabelitungNet => write!(f, "bangkabelitung-net"),
            Channel::CctruyenCom => write!(f, "cctruyen-com"),
            Channel::GetnadaCom => write!(f, "getnada-com"),
            Channel::GetnadaEmail => write!(f, "getnada-email"),
            Channel::GetnadaNet => write!(f, "getnada-net"),
            Channel::JawatengahNet => write!(f, "jawatengah-net"),
            Channel::JawatimurNet => write!(f, "jawatimur-net"),
            Channel::KalimantanbaratNet => write!(f, "kalimantanbarat-net"),
            Channel::KalimantanselatanNet => write!(f, "kalimantanselatan-net"),
            Channel::KalimantantengahNet => write!(f, "kalimantantengah-net"),
            Channel::KalimantantimurNet => write!(f, "kalimantantimur-net"),
            Channel::KalimantanutaraNet => write!(f, "kalimantanutara-net"),
            Channel::KepulauanriauNet => write!(f, "kepulauanriau-net"),
            Channel::Luxury345Com => write!(f, "luxury345-com"),
            Channel::MalukuutaraNet => write!(f, "malukuutara-net"),
            Channel::NusatenggarabaratNet => write!(f, "nusatenggarabarat-net"),
            Channel::NusatenggaratimurNet => write!(f, "nusatenggaratimur-net"),
            Channel::PapuabaratNet => write!(f, "papuabarat-net"),
            Channel::PapuabaratdayaNet => write!(f, "papuabaratdaya-net"),
            Channel::PapuaselatanNet => write!(f, "papuaselatan-net"),
            Channel::PeholCom => write!(f, "pehol-com"),
            Channel::PtruyenCom => write!(f, "ptruyen-com"),
            Channel::PulaubaliNet => write!(f, "pulaubali-net"),
            Channel::RiauNet => write!(f, "riau-net"),
            Channel::SeokeyOrg => write!(f, "seokey-org"),
            Channel::SulawesibaratNet => write!(f, "sulawesibarat-net"),
            Channel::SulawesiselatanNet => write!(f, "sulawesiselatan-net"),
            Channel::SulawesitengahNet => write!(f, "sulawesitengah-net"),
            Channel::SulawesitenggaraNet => write!(f, "sulawesitenggara-net"),
            Channel::SumaterabaratNet => write!(f, "sumaterabarat-net"),
            Channel::SumateraselatanNet => write!(f, "sumateraselatan-net"),
            Channel::SumaterautaraNet => write!(f, "sumaterautara-net"),
            Channel::VillatogelCom => write!(f, "villatogel-com"),
            Channel::Mail123 => write!(f, "mail123"),
            Channel::Mail10s => write!(f, "mail10s"),
            Channel::Webmailtemp => write!(f, "webmailtemp"),
            Channel::Tempfastmail => write!(f, "tempfastmail"),
            Channel::OneSecMail => write!(f, "1sec-mail"),
            Channel::Fakemail => write!(f, "fakemail"),
            Channel::Openinbox => write!(f, "openinbox"),
            Channel::Inboxes => write!(f, "inboxes"),
            Channel::Uncorreotemporal => write!(f, "uncorreotemporal"),
            Channel::Awamail => write!(f, "awamail"),
            Channel::MailTm => write!(f, "mail-tm"),
            Channel::WebLibraryNet => write!(f, "web-library-net"),
            Channel::Dropmail => write!(f, "dropmail"),
            Channel::GuerrillaMail => write!(f, "guerrillamail"),
            Channel::GuerrillamailCom => write!(f, "guerrillamail-com"),
            Channel::Maildrop => write!(f, "maildrop"),
            Channel::SmailPw => write!(f, "smail-pw"),
            Channel::Vip215 => write!(f, "vip-215"),
            Channel::FakeLegal => write!(f, "fake-legal"),
            Channel::ImguiDe => write!(f, "imgui-de"),
            Channel::PulsewebmenuDe => write!(f, "pulsewebmenu-de"),
            Channel::Moakt => write!(f, "moakt"),
            Channel::DrmailIn => write!(f, "drmail-in"),
            Channel::TemlNet => write!(f, "teml-net"),
            Channel::TmpemlCom => write!(f, "tmpeml-com"),
            Channel::TmpboxNet => write!(f, "tmpbox-net"),
            Channel::MoaktCc => write!(f, "moakt-cc"),
            Channel::DisboxNet => write!(f, "disbox-net"),
            Channel::TmpmailOrg => write!(f, "tmpmail-org"),
            Channel::TmpmailNet => write!(f, "tmpmail-net"),
            Channel::TmailsNet => write!(f, "tmails-net"),
            Channel::DisboxOrg => write!(f, "disbox-org"),
            Channel::MoaktCo => write!(f, "moakt-co"),
            Channel::MoaktWs => write!(f, "moakt-ws"),
            Channel::TmailWs => write!(f, "tmail-ws"),
            Channel::BareedWs => write!(f, "bareed-ws"),
            Channel::Email10min => write!(f, "email10min"),
            Channel::MjjCm => write!(f, "mjj-cm"),
            Channel::LinshiCo => write!(f, "linshi-co"),
            Channel::Harakirimail => write!(f, "harakirimail"),
            Channel::TempmailPlus => write!(f, "tempmail-plus"),
            Channel::FexpostCom => write!(f, "fexpost-com"),
            Channel::FexboxOrg => write!(f, "fexbox-org"),
            Channel::MailboxInUa => write!(f, "mailbox-in-ua"),
            Channel::RoverInfo => write!(f, "rover-info"),
            Channel::ChitthiIn => write!(f, "chitthi-in"),
            Channel::FextempCom => write!(f, "fextemp-com"),
            Channel::AnyPink => write!(f, "any-pink"),
            Channel::MerepostCom => write!(f, "merepost-com"),
            Channel::TempmailLolV2 => write!(f, "tempmail-lol-v2"),
            Channel::Tempgbox => write!(f, "tempgbox"),
            Channel::Emailnator => write!(f, "emailnator"),
            Channel::Temporam => write!(f, "temporam"),
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
            Channel::JqkjqkXyz => write!(f, "jqkjqk-xyz"),
            Channel::LyhleviCom => write!(f, "lyhlevi-com"),
            Channel::Neighbours => write!(f, "neighbours"),
            Channel::M2u => write!(f, "m2u"),
            Channel::TempyEmail => write!(f, "tempy-email"),
            Channel::Anonbox => write!(f, "anonbox"),
            Channel::Mailinator => write!(f, "mailinator"),
            Channel::Duckmail => write!(f, "duckmail"),
            Channel::Fmail => write!(f, "fmail"),
            Channel::Ockito => write!(f, "ockito"),
            Channel::Tempmail365 => write!(f, "tempmail365"),
            Channel::Tempinbox => write!(f, "tempinbox"),
            Channel::Byom => write!(f, "byom"),
            Channel::Anonymmail => write!(f, "anonymmail"),
            Channel::Eyepaste => write!(f, "eyepaste"),
            Channel::Segamail => write!(f, "segamail"),
            Channel::MailSunls => write!(f, "mail-sunls"),
            Channel::Expressinboxhub => write!(f, "expressinboxhub"),
            Channel::Lroid => write!(f, "lroid"),
            Channel::Haribu => write!(f, "haribu"),
            Channel::Pleasenospam => write!(f, "pleasenospam"),
            Channel::Rootsh => write!(f, "rootsh"),
            Channel::FakeEmailSite => write!(f, "fake-email-site"),
            Channel::Mohmal => write!(f, "mohmal"),
            Channel::Mailgolem => write!(f, "mailgolem"),
            Channel::BestTempMail => write!(f, "best-temp-mail"),
            Channel::DisposablemailApp => write!(f, "disposablemail-app"),
            Channel::MailtempCc => write!(f, "mailtemp-cc"),
            Channel::Minuteinbox => write!(f, "minuteinbox"),
            Channel::Mailcatch => write!(f, "mailcatch"),
            Channel::TempemailCo => write!(f, "tempemail-co"),
            Channel::TempemailsNet => write!(f, "tempemails-net"),
            Channel::Altmails => write!(f, "altmails"),
            Channel::TempemailInfo => write!(f, "tempemail-info"),
            Channel::Smailpro => write!(f, "smailpro"),
            Channel::Tempmailten => write!(f, "tempmailten"),
            Channel::MaildropCc => write!(f, "maildrop-cc"),
            Channel::TenminutemailNet => write!(f, "10minutemail-net"),
            Channel::LinshiyouxiangNet => write!(f, "linshiyouxiang-net"),
            Channel::TempmailFyi => write!(f, "tempmail-fyi"),
            Channel::DisposablemailCom => write!(f, "disposablemail-com"),
            Channel::TemppMails => write!(f, "tempp-mails"),
            Channel::EmailtempOrg => write!(f, "emailtemp-org"),
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
