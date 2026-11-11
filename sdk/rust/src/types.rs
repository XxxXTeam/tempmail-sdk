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
    #[serde(rename = "tempmail-fish")]
    TempmailFish,
    #[serde(rename = "neighbours-sh")]
    NeighboursSh,
    #[serde(rename = "mailnesia")]
    Mailnesia,
    /// mailticking.com（旧域名 temporary-mail.net 更名站）临时邮箱
    #[serde(rename = "mailticking")]
    Mailticking,
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
    #[serde(rename = "dropmail-me")]
    DropmailMe,
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
    #[serde(rename = "sogetthis-com")]
    SogetthisCom,
    #[serde(rename = "bobmail-info")]
    BobmailInfo,
    #[serde(rename = "suremail-info")]
    SuremailInfo,
    #[serde(rename = "binkmail-com")]
    BinkmailCom,
    #[serde(rename = "veryrealemail-com")]
    VeryrealemailCom,
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
    #[serde(rename = "mail-sunls")]
    MailSunls,
    #[serde(rename = "expressinboxhub")]
    Expressinboxhub,
    #[serde(rename = "lroid")]
    Lroid,
    #[serde(rename = "haribu")]
    Haribu,
    #[serde(rename = "rootsh")]
    Rootsh,
    #[serde(rename = "fake-email-site")]
    FakeEmailSite,
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
    /// mytempmail.cc 渠道
    #[serde(rename = "mytempmail-cc")]
    MytempmailCc,
    /// mail.td 渠道
    #[serde(rename = "mail-td")]
    MailTd,
    /// mailhole.de 渠道
    #[serde(rename = "mailhole-de")]
    MailholeDe,
    /// tmail.link 渠道
    #[serde(rename = "tmail-link")]
    TmailLink,
    /// 24mail.chacuo.net 渠道
    #[serde(rename = "24mail-chacuo")]
    TwentyfourmailChacuo,
    /// nimail.cn 渠道
    #[serde(rename = "nimail")]
    Nimail,
    /// freecustom.email 渠道
    #[serde(rename = "freecustom")]
    Freecustom,
    /// apihz（接口盒子）渠道
    #[serde(rename = "apihz")]
    Apihz,
    /// mailmomy.com 渠道
    #[serde(rename = "mailmomy")]
    Mailmomy,
    /// mailinator 姊妹域名 chammy.info
    #[serde(rename = "chammy-info")]
    ChammyInfo,
    /// mailinator 姊妹域名 thisisnotmyrealemail.com
    #[serde(rename = "thisisnotmyrealemail-com")]
    ThisisnotmyrealemailCom,
    /// mailinator 姊妹域名 notmailinator.com
    #[serde(rename = "notmailinator-com")]
    NotmailinatorCom,
    /// mailinator 姊妹域名 spamhereplease.com
    #[serde(rename = "spamhereplease-com")]
    SpamherepleaseCom,
    /// mailinator 姊妹域名 sendspamhere.com
    #[serde(rename = "sendspamhere-com")]
    SendspamhereCom,
    /// mailinator 姊妹域名 sendfree.org
    #[serde(rename = "sendfree-org")]
    SendfreeOrg,
    /// mailinator 姊妹域名 junk.beats.org
    #[serde(rename = "junk-beats-org")]
    JunkBeatsOrg,
    /// mailinator 姊妹域名 junk.ihmehl.com
    #[serde(rename = "junk-ihmehl-com")]
    JunkIhmehlCom,
    /// mailinator 姊妹域名 junk.noplay.org
    #[serde(rename = "junk-noplay-org")]
    JunkNoplayOrg,
    /// mailinator 姊妹域名 junk.vanillasystem.com
    #[serde(rename = "junk-vanillasystem-com")]
    JunkVanillasystemCom,
    /// mailinator 姊妹域名 spam.jasonpearce.com
    #[serde(rename = "spam-jasonpearce-com")]
    SpamJasonpearceCom,
    /// mailinator 姊妹域名 fish.skytale.net
    #[serde(rename = "fish-skytale-net")]
    FishSkytaleNet,
    /// mailinator 姊妹域名 spam.mccrew.com
    #[serde(rename = "spam-mccrew-com")]
    SpamMccrewCom,
    /// mailinator 姊妹域名 spam.coroiu.com
    #[serde(rename = "spam-coroiu-com")]
    SpamCoroiuCom,
    /// mailinator 姊妹域名 spam.deluser.net
    #[serde(rename = "spam-deluser-net")]
    SpamDeluserNet,
    /// mailinator 姊妹域名 spam.dhsf.net
    #[serde(rename = "spam-dhsf-net")]
    SpamDhsfNet,
    /// mailinator 姊妹域名 spam.lucatnt.com
    #[serde(rename = "spam-lucatnt-com")]
    SpamLucatntCom,
    /// mailinator 姊妹域名 spam.lyceum-life.com.ru
    #[serde(rename = "spam-lyceum-life-com-ru")]
    SpamLyceumLifeComRu,
    /// mailinator 姊妹域名 spam.netpirates.net
    #[serde(rename = "spam-netpirates-net")]
    SpamNetpiratesNet,
    /// mailinator 姊妹域名 spam.no-ip.net
    #[serde(rename = "spam-no-ip-net")]
    SpamNoIpNet,
    /// mailinator 姊妹域名 spam.ozh.org
    #[serde(rename = "spam-ozh-org")]
    SpamOzhOrg,
    /// mailinator 姊妹域名 spam.pyphus.org
    #[serde(rename = "spam-pyphus-org")]
    SpamPyphusOrg,
    /// mailinator 姊妹域名 spam.shep.pw
    #[serde(rename = "spam-shep-pw")]
    SpamShepPw,
    /// mailinator 姊妹域名 spam.wtf.at
    #[serde(rename = "spam-wtf-at")]
    SpamWtfAt,
    /// mailinator 姊妹域名 spam.wulczer.org
    #[serde(rename = "spam-wulczer-org")]
    SpamWulczerOrg,
    #[serde(rename = "crap-kakadua-net")]
    CrapKakaduaNet,
    #[serde(rename = "spam-janlugt-nl")]
    SpamJanlugtNl,
    #[serde(rename = "min-burningfish-net")]
    MinBurningfishNet,
    #[serde(rename = "sink-fblay-com")]
    SinkFblayCom,
    #[serde(rename = "etgdev-de")]
    EtgdevDe,
    #[serde(rename = "mtmdev-com")]
    MtmdevCom,
    #[serde(rename = "test-unergie-com")]
    TestUnergieCom,
    #[serde(rename = "block-bdea-cc")]
    BlockBdeaCc,
    #[serde(rename = "torch-yi-org")]
    TorchYiOrg,
    #[serde(rename = "carlton183-changeip-net")]
    Carlton183ChangeipNet,
    #[serde(rename = "mail-fsmash-org")]
    MailFsmashOrg,
    #[serde(rename = "ebs-com-ar")]
    EbsComAr,
    #[serde(rename = "jama-trenet-eu")]
    JamaTrenetEu,
    #[serde(rename = "blackhole-djurby-se")]
    BlackholeDjurbySe,
    #[serde(rename = "m8r-davidfuhr-de")]
    M8rDavidfuhrDe,
    #[serde(rename = "mi-meon-be")]
    MiMeonBe,
    #[serde(rename = "m-nik-me")]
    MNikMe,
    #[serde(rename = "mail-bentrask-com")]
    MailBentraskCom,
    #[serde(rename = "t-zibet-net")]
    TZibetNet,
    #[serde(rename = "m8r-mcasal-com")]
    M8rMcasalCom,
    #[serde(rename = "ramjane-mooo-com")]
    RamjaneMoooCom,
    #[serde(rename = "rauxa-seny-cat")]
    RauxaSenyCat,
    #[serde(rename = "sp-woot-at")]
    SpWootAt,
    #[serde(rename = "fwd2m-eszett-es")]
    Fwd2mEszettEs,
    #[serde(rename = "m-887-at")]
    M887At,
    #[serde(rename = "nospam-thurstons-us")]
    NospamThurstonsUs,
    #[serde(rename = "null-k3vin-net")]
    NullK3vinNet,
    #[serde(rename = "really-istrash-com")]
    ReallyIstrashCom,
    #[serde(rename = "spam-hortuk-ovh")]
    SpamHortukOvh,
    /// dropmail.click 渠道
    #[serde(rename = "dropmail-click")]
    DropmailClick,
    #[serde(rename = "17666688-shop")]
    N17666688Shop,
    #[serde(rename = "282mail-com")]
    N282mailCom,
    #[serde(rename = "bsdu32-buzz")]
    Bsdu32Buzz,
    #[serde(rename = "b-smelly-cc")]
    BSmellyCc,
    #[serde(rename = "dea-soon-it")]
    DeaSoonIt,
    #[serde(rename = "disposable-al-sudani-com")]
    DisposableAlSudaniCom,
    #[serde(rename = "disposable-nogonad-nl")]
    DisposableNogonadNl,
    #[serde(rename = "doxu243-buzz")]
    Doxu243Buzz,
    #[serde(rename = "easyme-pro")]
    EasymePro,
    #[serde(rename = "j-fairuse-org")]
    JFairuseOrg,
    #[serde(rename = "layueming-pics")]
    LayuemingPics,
    #[serde(rename = "mailinatorzz-mooo-com")]
    MailinatorzzMoooCom,
    #[serde(rename = "mingyuekeji-online")]
    MingyuekejiOnline,
    #[serde(rename = "mingyueming-click")]
    MingyuemingClick,
    #[serde(rename = "mingyueming-shop")]
    MingyuemingShop,
    #[serde(rename = "mingyukeji-lol")]
    MingyukejiLol,
    #[serde(rename = "mn-curppa-com")]
    MnCurppaCom,
    #[serde(rename = "notfond-404-mn")]
    Notfond404Mn,
    #[serde(rename = "nuxh62-space")]
    Nuxh62Space,
    #[serde(rename = "proid-cloud-ip-cc")]
    ProidCloudIpCc,
    #[serde(rename = "sbook-pics")]
    SbookPics,
    #[serde(rename = "xue32-buzz")]
    Xue32Buzz,
    #[serde(rename = "tempgmailer")]
    Tempgmailer,
    /// temp-mail.org 临时邮箱
    #[serde(rename = "temp-mail-org")]
    TempMailOrg,
    /// xkx.me 临时邮箱
    #[serde(rename = "xkx-me")]
    XkxMe,
    /// mailcat.ai 临时邮箱
    #[serde(rename = "mailcat-ai")]
    MailcatAi,
    /// tempgo.email 临时邮箱
    #[serde(rename = "tempgo-email")]
    TempgoEmail,
    /// restmail.net ad-hoc 临时邮箱
    #[serde(rename = "restmail-net")]
    RestmailNet,
    /// 10minutemail.net PHP session 临时邮箱
    #[serde(rename = "ten-minute-mail-net")]
    TenMinuteMailNet,
    /// tempmails.io 无认证 REST 临时邮箱
    #[serde(rename = "tempmails-io")]
    TempmailsIo,
    /// shitpost.email 公共实例临时邮箱
    #[serde(rename = "shitpost-email")]
    ShitpostEmail,
    /// smails.dev 临时邮箱
    #[serde(rename = "smails")]
    Smails,
    /// tempmailportal.com 临时邮箱
    #[serde(rename = "tempmailportal")]
    Tempmailportal,
    /// huskmail.xyz 临时邮箱
    #[serde(rename = "huskmail")]
    Huskmail,
    /// zerodrop.dev 临时邮箱
    #[serde(rename = "zerodrop")]
    Zerodrop,
    /// firetempmail.com 临时邮箱
    #[serde(rename = "firetempmail")]
    Firetempmail,
    /// nullmail.cc 临时邮箱
    #[serde(rename = "nullmail")]
    Nullmail,
    /// tenmin.app 临时邮箱
    #[serde(rename = "tenmin-app")]
    TenminApp,
    /// mtempmail.com 临时邮箱
    #[serde(rename = "mtempmail")]
    Mtempmail,
    /// tempmail.ee 临时邮箱
    #[serde(rename = "tempmail-ee")]
    TempmailEe,
    /// temporarymail.com 临时邮箱
    #[serde(rename = "temporarymail-com")]
    TemporarymailCom,
    /// 30minemail.com 临时邮箱
    #[serde(rename = "30minemail")]
    Email30Min,
    /// linshi.xyz 临时邮箱
    #[serde(rename = "linshi-xyz")]
    LinshiXyz,
    /// crazymailing.com 临时邮箱
    #[serde(rename = "crazymailing")]
    Crazymailing,
    /// tempmail.noxen.de5.net UniMail-Bot 公共实例
    #[serde(rename = "noxen-de5-net")]
    NoxenDe5Net,
    /// nukemail.app 临时邮箱
    #[serde(rename = "nukemail")]
    Nukemail,
    /// shadowmail.win 临时邮箱
    #[serde(rename = "shadowmail")]
    Shadowmail,
    /// flybymail.com 临时邮箱
    #[serde(rename = "flybymail")]
    Flybymail,
    /// nowtempmail.com 临时邮箱
    #[serde(rename = "nowtempmail")]
    Nowtempmail,
    /// clawdemail.com 临时邮箱
    #[serde(rename = "clawdemail")]
    Clawdemail,
    /// tempmail100.com 临时邮箱
    #[serde(rename = "tempmail100")]
    Tempmail100,
    /// tempmailto.com 临时邮箱
    #[serde(rename = "tempmailto")]
    Tempmailto,
    /// temp-mail.gg 临时邮箱
    #[serde(rename = "temp-mail-gg")]
    TempMailGg,
    /// tmpkit.com（Next.js tRPC 会话）临时邮箱
    #[serde(rename = "tmpkit")]
    Tmpkit,
    /// internxt.com（临时邮箱页 Cookie 会话 + CSRF）临时邮箱
    #[serde(rename = "internxt")]
    Internxt,
    /// generator.email（SSR 网页型，SITE_DATA 快照建箱）临时邮箱
    #[serde(rename = "generator-email")]
    GeneratorEmail,
    /// inboxes.com 服务方镜像域名 blondmail.com
    #[serde(rename = "blondmail-com")]
    BlondmailCom,
    /// inboxes.com 服务方镜像域名 chapsmail.com
    #[serde(rename = "chapsmail-com")]
    ChapsmailCom,
    /// inboxes.com 服务方镜像域名 clowmail.com
    #[serde(rename = "clowmail-com")]
    ClowmailCom,
    /// inboxes.com 服务方镜像域名 dropjar.com
    #[serde(rename = "dropjar-com")]
    DropjarCom,
    /// inboxes.com 服务方镜像域名 fivermail.com
    #[serde(rename = "fivermail-com")]
    FivermailCom,
    /// inboxes.com 服务方镜像域名 getairmail.com
    #[serde(rename = "getairmail-com")]
    GetairmailCom,
    /// inboxes.com 服务方镜像域名 getmule.com
    #[serde(rename = "getmule-com")]
    GetmuleCom,
    /// inboxes.com 服务方镜像域名 inboxbear.com
    #[serde(rename = "inboxbear-com")]
    InboxbearCom,
    /// inboxes.com 服务方镜像域名 replyloop.com
    #[serde(rename = "replyloop-com")]
    ReplyloopCom,
    /// inboxes.com 服务方镜像域名 temptami.com
    #[serde(rename = "temptami-com")]
    TemptamiCom,
    /// inboxes.com 服务方镜像域名 gimpmail.com
    #[serde(rename = "gimpmail-com")]
    GimpmailCom,
    /// inboxes.com 服务方镜像域名 givmail.com
    #[serde(rename = "givmail-com")]
    GivmailCom,
    /// inboxes.com 服务方镜像域名 guysmail.com
    #[serde(rename = "guysmail-com")]
    GuysmailCom,
    /// inboxes.com 服务方镜像域名 robot-mail.com
    #[serde(rename = "robot-mail-com")]
    RobotMailCom,
    /// inboxes.com 服务方镜像域名 tupmail.com
    #[serde(rename = "tupmail-com")]
    TupmailCom,
    /// inboxes.com 服务方镜像域名 vomoto.com
    #[serde(rename = "vomoto-com")]
    VomotoCom,
    /// mail.cx 服务方镜像域名 uqu.me
    #[serde(rename = "uqu-me")]
    UquMe,
    /// mail.cx 服务方镜像域名 9k3r.com
    #[serde(rename = "9k3r-com")]
    N9k3rCom,
    /// mailmomy 服务方镜像域名 2famail.com
    #[serde(rename = "2famail-com")]
    N2famailCom,
    /// mailmomy 服务方镜像域名 xikemail.com
    #[serde(rename = "xikemail-com")]
    XikemailCom,
    /// mail.tm 服务方镜像域名 fldubai.com
    #[serde(rename = "fldubai-com")]
    FldubaiCom,
    /// duckmail 服务方镜像域名 niceground.shop
    #[serde(rename = "niceground-shop")]
    NicegroundShop,
    /// duckmail 服务方镜像域名 glasswhitehub.com
    #[serde(rename = "glasswhitehub-com")]
    GlasswhitehubCom,
    /// duckmail 服务方镜像域名 hubaiclass.org
    #[serde(rename = "hubaiclass-org")]
    HubaiclassOrg,
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
            Channel::TempmailFish => write!(f, "tempmail-fish"),
            Channel::NeighboursSh => write!(f, "neighbours-sh"),
            Channel::Mailnesia => write!(f, "mailnesia"),
            Channel::Mailticking => write!(f, "mailticking"),
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
            Channel::DropmailMe => write!(f, "dropmail-me"),
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
            Channel::SogetthisCom => write!(f, "sogetthis-com"),
            Channel::BobmailInfo => write!(f, "bobmail-info"),
            Channel::SuremailInfo => write!(f, "suremail-info"),
            Channel::BinkmailCom => write!(f, "binkmail-com"),
            Channel::VeryrealemailCom => write!(f, "veryrealemail-com"),
            Channel::Duckmail => write!(f, "duckmail"),
            Channel::Fmail => write!(f, "fmail"),
            Channel::Ockito => write!(f, "ockito"),
            Channel::Tempmail365 => write!(f, "tempmail365"),
            Channel::Tempinbox => write!(f, "tempinbox"),
            Channel::Byom => write!(f, "byom"),
            Channel::Anonymmail => write!(f, "anonymmail"),
            Channel::Eyepaste => write!(f, "eyepaste"),
            Channel::MailSunls => write!(f, "mail-sunls"),
            Channel::Expressinboxhub => write!(f, "expressinboxhub"),
            Channel::Lroid => write!(f, "lroid"),
            Channel::Haribu => write!(f, "haribu"),
            Channel::Rootsh => write!(f, "rootsh"),
            Channel::FakeEmailSite => write!(f, "fake-email-site"),
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
            Channel::MytempmailCc => write!(f, "mytempmail-cc"),
            Channel::MailTd => write!(f, "mail-td"),
            Channel::MailholeDe => write!(f, "mailhole-de"),
            Channel::TmailLink => write!(f, "tmail-link"),
            Channel::TwentyfourmailChacuo => write!(f, "24mail-chacuo"),
            Channel::Nimail => write!(f, "nimail"),
            Channel::Freecustom => write!(f, "freecustom"),
            Channel::Apihz => write!(f, "apihz"),
            Channel::Mailmomy => write!(f, "mailmomy"),
            Channel::ChammyInfo => write!(f, "chammy-info"),
            Channel::ThisisnotmyrealemailCom => write!(f, "thisisnotmyrealemail-com"),
            Channel::NotmailinatorCom => write!(f, "notmailinator-com"),
            Channel::SpamherepleaseCom => write!(f, "spamhereplease-com"),
            Channel::SendspamhereCom => write!(f, "sendspamhere-com"),
            Channel::SendfreeOrg => write!(f, "sendfree-org"),
            Channel::JunkBeatsOrg => write!(f, "junk-beats-org"),
            Channel::JunkIhmehlCom => write!(f, "junk-ihmehl-com"),
            Channel::JunkNoplayOrg => write!(f, "junk-noplay-org"),
            Channel::JunkVanillasystemCom => write!(f, "junk-vanillasystem-com"),
            Channel::SpamJasonpearceCom => write!(f, "spam-jasonpearce-com"),
            Channel::FishSkytaleNet => write!(f, "fish-skytale-net"),
            Channel::SpamMccrewCom => write!(f, "spam-mccrew-com"),
            Channel::SpamCoroiuCom => write!(f, "spam-coroiu-com"),
            Channel::SpamDeluserNet => write!(f, "spam-deluser-net"),
            Channel::SpamDhsfNet => write!(f, "spam-dhsf-net"),
            Channel::SpamLucatntCom => write!(f, "spam-lucatnt-com"),
            Channel::SpamLyceumLifeComRu => write!(f, "spam-lyceum-life-com-ru"),
            Channel::SpamNetpiratesNet => write!(f, "spam-netpirates-net"),
            Channel::SpamNoIpNet => write!(f, "spam-no-ip-net"),
            Channel::SpamOzhOrg => write!(f, "spam-ozh-org"),
            Channel::SpamPyphusOrg => write!(f, "spam-pyphus-org"),
            Channel::SpamShepPw => write!(f, "spam-shep-pw"),
            Channel::SpamWtfAt => write!(f, "spam-wtf-at"),
            Channel::SpamWulczerOrg => write!(f, "spam-wulczer-org"),
            Channel::CrapKakaduaNet => write!(f, "crap-kakadua-net"),
            Channel::SpamJanlugtNl => write!(f, "spam-janlugt-nl"),
            Channel::MinBurningfishNet => write!(f, "min-burningfish-net"),
            Channel::SinkFblayCom => write!(f, "sink-fblay-com"),
            Channel::EtgdevDe => write!(f, "etgdev-de"),
            Channel::MtmdevCom => write!(f, "mtmdev-com"),
            Channel::TestUnergieCom => write!(f, "test-unergie-com"),
            Channel::BlockBdeaCc => write!(f, "block-bdea-cc"),
            Channel::TorchYiOrg => write!(f, "torch-yi-org"),
            Channel::Carlton183ChangeipNet => write!(f, "carlton183-changeip-net"),
            Channel::MailFsmashOrg => write!(f, "mail-fsmash-org"),
            Channel::EbsComAr => write!(f, "ebs-com-ar"),
            Channel::JamaTrenetEu => write!(f, "jama-trenet-eu"),
            Channel::BlackholeDjurbySe => write!(f, "blackhole-djurby-se"),
            Channel::M8rDavidfuhrDe => write!(f, "m8r-davidfuhr-de"),
            Channel::MiMeonBe => write!(f, "mi-meon-be"),
            Channel::MNikMe => write!(f, "m-nik-me"),
            Channel::MailBentraskCom => write!(f, "mail-bentrask-com"),
            Channel::TZibetNet => write!(f, "t-zibet-net"),
            Channel::M8rMcasalCom => write!(f, "m8r-mcasal-com"),
            Channel::RamjaneMoooCom => write!(f, "ramjane-mooo-com"),
            Channel::RauxaSenyCat => write!(f, "rauxa-seny-cat"),
            Channel::SpWootAt => write!(f, "sp-woot-at"),
            Channel::Fwd2mEszettEs => write!(f, "fwd2m-eszett-es"),
            Channel::M887At => write!(f, "m-887-at"),
            Channel::NospamThurstonsUs => write!(f, "nospam-thurstons-us"),
            Channel::NullK3vinNet => write!(f, "null-k3vin-net"),
            Channel::ReallyIstrashCom => write!(f, "really-istrash-com"),
            Channel::SpamHortukOvh => write!(f, "spam-hortuk-ovh"),
            Channel::DropmailClick => write!(f, "dropmail-click"),
            Channel::N17666688Shop => write!(f, "17666688-shop"),
            Channel::N282mailCom => write!(f, "282mail-com"),
            Channel::Bsdu32Buzz => write!(f, "bsdu32-buzz"),
            Channel::BSmellyCc => write!(f, "b-smelly-cc"),
            Channel::DeaSoonIt => write!(f, "dea-soon-it"),
            Channel::DisposableAlSudaniCom => write!(f, "disposable-al-sudani-com"),
            Channel::DisposableNogonadNl => write!(f, "disposable-nogonad-nl"),
            Channel::Doxu243Buzz => write!(f, "doxu243-buzz"),
            Channel::EasymePro => write!(f, "easyme-pro"),
            Channel::JFairuseOrg => write!(f, "j-fairuse-org"),
            Channel::LayuemingPics => write!(f, "layueming-pics"),
            Channel::MailinatorzzMoooCom => write!(f, "mailinatorzz-mooo-com"),
            Channel::MingyuekejiOnline => write!(f, "mingyuekeji-online"),
            Channel::MingyuemingClick => write!(f, "mingyueming-click"),
            Channel::MingyuemingShop => write!(f, "mingyueming-shop"),
            Channel::MingyukejiLol => write!(f, "mingyukeji-lol"),
            Channel::MnCurppaCom => write!(f, "mn-curppa-com"),
            Channel::Notfond404Mn => write!(f, "notfond-404-mn"),
            Channel::Nuxh62Space => write!(f, "nuxh62-space"),
            Channel::ProidCloudIpCc => write!(f, "proid-cloud-ip-cc"),
            Channel::SbookPics => write!(f, "sbook-pics"),
            Channel::Xue32Buzz => write!(f, "xue32-buzz"),
            Channel::Tempgmailer => write!(f, "tempgmailer"),
            Channel::TempMailOrg => write!(f, "temp-mail-org"),
            Channel::XkxMe => write!(f, "xkx-me"),
            Channel::MailcatAi => write!(f, "mailcat-ai"),
            Channel::TempgoEmail => write!(f, "tempgo-email"),
            Channel::RestmailNet => write!(f, "restmail-net"),
            Channel::TenMinuteMailNet => write!(f, "ten-minute-mail-net"),
            Channel::TempmailsIo => write!(f, "tempmails-io"),
            Channel::ShitpostEmail => write!(f, "shitpost-email"),
            Channel::Smails => write!(f, "smails"),
            Channel::Tempmailportal => write!(f, "tempmailportal"),
            Channel::Huskmail => write!(f, "huskmail"),
            Channel::Zerodrop => write!(f, "zerodrop"),
            Channel::Firetempmail => write!(f, "firetempmail"),
            Channel::Nullmail => write!(f, "nullmail"),
            Channel::TenminApp => write!(f, "tenmin-app"),
            Channel::Mtempmail => write!(f, "mtempmail"),
            Channel::TempmailEe => write!(f, "tempmail-ee"),
            Channel::TemporarymailCom => write!(f, "temporarymail-com"),
            Channel::Email30Min => write!(f, "30minemail"),
            Channel::LinshiXyz => write!(f, "linshi-xyz"),
            Channel::Crazymailing => write!(f, "crazymailing"),
            Channel::NoxenDe5Net => write!(f, "noxen-de5-net"),
            Channel::Nukemail => write!(f, "nukemail"),
            Channel::Shadowmail => write!(f, "shadowmail"),
            Channel::Flybymail => write!(f, "flybymail"),
            Channel::Nowtempmail => write!(f, "nowtempmail"),
            Channel::Clawdemail => write!(f, "clawdemail"),
            Channel::Tempmail100 => write!(f, "tempmail100"),
            Channel::Tempmailto => write!(f, "tempmailto"),
            Channel::TempMailGg => write!(f, "temp-mail-gg"),
            Channel::Tmpkit => write!(f, "tmpkit"),
            Channel::Internxt => write!(f, "internxt"),
            Channel::GeneratorEmail => write!(f, "generator-email"),
            Channel::BlondmailCom => write!(f, "blondmail-com"),
            Channel::ChapsmailCom => write!(f, "chapsmail-com"),
            Channel::ClowmailCom => write!(f, "clowmail-com"),
            Channel::DropjarCom => write!(f, "dropjar-com"),
            Channel::FivermailCom => write!(f, "fivermail-com"),
            Channel::GetairmailCom => write!(f, "getairmail-com"),
            Channel::GetmuleCom => write!(f, "getmule-com"),
            Channel::InboxbearCom => write!(f, "inboxbear-com"),
            Channel::ReplyloopCom => write!(f, "replyloop-com"),
            Channel::TemptamiCom => write!(f, "temptami-com"),
            Channel::GimpmailCom => write!(f, "gimpmail-com"),
            Channel::GivmailCom => write!(f, "givmail-com"),
            Channel::GuysmailCom => write!(f, "guysmail-com"),
            Channel::RobotMailCom => write!(f, "robot-mail-com"),
            Channel::TupmailCom => write!(f, "tupmail-com"),
            Channel::VomotoCom => write!(f, "vomoto-com"),
            Channel::UquMe => write!(f, "uqu-me"),
            Channel::N9k3rCom => write!(f, "9k3r-com"),
            Channel::N2famailCom => write!(f, "2famail-com"),
            Channel::XikemailCom => write!(f, "xikemail-com"),
            Channel::FldubaiCom => write!(f, "fldubai-com"),
            Channel::NicegroundShop => write!(f, "niceground-shop"),
            Channel::GlasswhitehubCom => write!(f, "glasswhitehub-com"),
            Channel::HubaiclassOrg => write!(f, "hubaiclass-org"),
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
    /// 最大尝试渠道数，默认 20
    pub max_channels_tried: Option<u32>,
    /// 整体超时时间（秒），默认 60
    pub total_timeout: Option<f64>,
    /// 精确匹配邮箱后缀（如 "@gmail.com" 或 "gmail.com"），只尝试支持该域名的渠道
    pub suffix: Option<String>,
    /// 支持多个域名匹配（如 vec!["outlook.com", "hotmail.com"]），只尝试支持这些域名的渠道
    pub domains: Option<Vec<String>>,
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
