/*!
 * 渠道域名映射模块
 * 用于根据邮箱后缀/域名筛选可用渠道
 */

use std::collections::{HashMap, HashSet};
use std::sync::LazyLock;

use crate::types::Channel;

/// 渠道到已知固定域名的映射表
static CHANNEL_DOMAINS: LazyLock<HashMap<Channel, Vec<&'static str>>> = LazyLock::new(|| {
    let mut m: HashMap<Channel, Vec<&'static str>> = HashMap::new();
    m.insert(Channel::Emailnator, vec!["gmail.com", "googlemail.com"]);
    m.insert(Channel::Tempgmailer, vec!["gmail.com"]);
    m.insert(Channel::Catchmail, vec!["catchmail.io", "mailistry.com", "zeppost.com"]);
    m.insert(Channel::CatchmailMailistry, vec!["mailistry.com"]);
    m.insert(Channel::CatchmailZeppost, vec!["zeppost.com"]);
    m.insert(Channel::Mailforspam, vec!["mailforspam.com", "tempmail.io", "disposable.email"]);
    m.insert(Channel::MailforspamTempmailIo, vec!["tempmail.io"]);
    m.insert(Channel::MailforspamDisposable, vec!["disposable.email"]);
    m.insert(Channel::Tempmail365, vec!["fengyou.cc", "shop345.com", "nutemail.com", "qvrf.cn"]);
    m.insert(Channel::Tempinbox, vec!["tempinbox.xyz", "thepiratebay.cloud", "cryptoblad.nl"]);
    m.insert(Channel::TenminuteOne, vec!["xghff.com", "oqqaj.com", "psovv.com", "dbwot.com", "ygwpr.com", "imxwe.com"]);
    m.insert(Channel::XghffCom, vec!["xghff.com"]);
    m.insert(Channel::OqqajCom, vec!["oqqaj.com"]);
    m.insert(Channel::PsovvCom, vec!["psovv.com"]);
    m.insert(Channel::DbwotCom, vec!["dbwot.com"]);
    m.insert(Channel::YgwprCom, vec!["ygwpr.com"]);
    m.insert(Channel::ImxweCom, vec!["imxwe.com"]);
    m.insert(Channel::Apihz, vec!["apimail.email", "apimail.vip"]);
    m.insert(Channel::TwentyfourmailChacuo, vec!["chacuo.net", "027168.com"]);
    m.insert(Channel::Mailinator, vec!["mailinator.com"]);
    m.insert(Channel::SogetthisCom, vec!["sogetthis.com"]);
    m.insert(Channel::BobmailInfo, vec!["bobmail.info"]);
    m.insert(Channel::SuremailInfo, vec!["suremail.info"]);
    m.insert(Channel::BinkmailCom, vec!["binkmail.com"]);
    m.insert(Channel::VeryrealemailCom, vec!["veryrealemail.com"]);
    m.insert(Channel::ChammyInfo, vec!["chammy.info"]);
    m.insert(Channel::ThisisnotmyrealemailCom, vec!["thisisnotmyrealemail.com"]);
    m.insert(Channel::NotmailinatorCom, vec!["notmailinator.com"]);
    m.insert(Channel::SpamherepleaseCom, vec!["spamhereplease.com"]);
    m.insert(Channel::SendspamhereCom, vec!["sendspamhere.com"]);
    m.insert(Channel::SendfreeOrg, vec!["sendfree.org"]);
    m.insert(Channel::Mailnesia, vec!["mailnesia.com"]);
    m.insert(Channel::Harakirimail, vec!["harakirimail.com"]);
    m.insert(Channel::Byom, vec!["byom.de"]);
    m.insert(Channel::Eyepaste, vec!["eyepaste.com"]);
    m.insert(Channel::Mailcatch, vec!["mailcatch.com"]);
    m.insert(Channel::MaildropCc, vec!["maildrop.cc"]);
    m.insert(Channel::SmailPw, vec!["smail.pw"]);
    m.insert(Channel::Inboxkitten, vec!["inboxkitten.com"]);
    m.insert(Channel::Mffac, vec!["mffac.com"]);
    m.insert(Channel::Nimail, vec!["nimail.cn"]);
    m.insert(Channel::Mailgolem, vec!["mailgolem.com"]);
    m.insert(Channel::TempmailPlus, vec!["mailto.plus"]);
    m.insert(Channel::FexpostCom, vec!["fexpost.com"]);
    m.insert(Channel::FexboxOrg, vec!["fexbox.org"]);
    m.insert(Channel::MailboxInUa, vec!["mailbox.in.ua"]);
    m.insert(Channel::RoverInfo, vec!["rover.info"]);
    m.insert(Channel::ChitthiIn, vec!["chitthi.in"]);
    m.insert(Channel::FextempCom, vec!["fextemp.com"]);
    m.insert(Channel::AnyPink, vec!["any.pink"]);
    m.insert(Channel::MerepostCom, vec!["merepost.com"]);
    m.insert(Channel::Getnada, vec!["getnada.com"]);
    m.insert(Channel::GetnadaCom, vec!["getnada.com"]);
    m.insert(Channel::GetnadaEmail, vec!["getnada.email"]);
    m.insert(Channel::GetnadaNet, vec!["getnada.net"]);
    m.insert(Channel::DisposablemailApp, vec!["disposablemail.dev", "mailmehere.cc"]);
    m.insert(Channel::Minuteinbox, vec!["minafter.com"]);
    m.insert(Channel::Mohmal, vec!["emailinbo.live"]);
    m
});

/// 动态域名渠道集合（这些渠道的域名列表是动态获取的，筛选时默认保留）
static DYNAMIC_DOMAIN_CHANNELS: LazyLock<HashSet<Channel>> = LazyLock::new(|| {
    let mut s = HashSet::new();
    s.insert(Channel::MailTm);
    s.insert(Channel::Duckmail);
    s.insert(Channel::MailTd);
    s.insert(Channel::ChatgptOrgUk);
    s.insert(Channel::Temporam);
    s.insert(Channel::Getnada);
    s.insert(Channel::Neighbours);
    s.insert(Channel::NeighboursSh);
    s.insert(Channel::Inboxes);
    s.insert(Channel::Mailmomy);
    s.insert(Channel::Freecustom);
    s.insert(Channel::MailSunls);
    s.insert(Channel::Anonymmail);
    s.insert(Channel::FakeLegal);
    s.insert(Channel::Maildrop);
    s.insert(Channel::MailCx);
    s.insert(Channel::Moakt);
    s.insert(Channel::TempmailLol);
    s.insert(Channel::Smailpro);
    s.insert(Channel::Tempmail);
    s.insert(Channel::OneSecMail);
    s.insert(Channel::GuerrillaMail);
    s.insert(Channel::Dropmail);
    s.insert(Channel::Fmail);
    s.insert(Channel::Ockito);
    s.insert(Channel::TempMailOrg);
    s
});

/// 根据目标域名筛选渠道列表
///
/// 如果 target_domains 为空，返回原渠道列表。
/// 动态域名渠道默认保留（因为无法提前确定其支持的域名）。
/// 如果筛选结果为空，返回原渠道列表（避免完全无法使用）。
pub fn filter_channels_by_domain(channels: &[Channel], target_domains: &[String]) -> Vec<Channel> {
    if target_domains.is_empty() {
        return channels.to_vec();
    }

    let filtered: Vec<Channel> = channels
        .iter()
        .filter(|ch| {
            // 动态域名渠道默认保留
            if DYNAMIC_DOMAIN_CHANNELS.contains(ch) {
                return true;
            }
            // 检查渠道已知域名是否与目标匹配
            if let Some(domains) = CHANNEL_DOMAINS.get(ch) {
                return domains.iter().any(|cd| {
                    target_domains
                        .iter()
                        .any(|td| *cd == td.as_str() || cd.ends_with(&format!(".{}", td)))
                });
            }
            false
        })
        .cloned()
        .collect();

    // 筛选结果为空时回退到原列表
    if filtered.is_empty() {
        channels.to_vec()
    } else {
        filtered
    }
}
