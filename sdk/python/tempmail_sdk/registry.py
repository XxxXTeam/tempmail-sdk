"""
渠道注册表：单一事实来源

每新增一个渠道只需在本文件追加一处 register_channel(ChannelSpec(...))，
渠道列表（ALL_CHANNELS）、信息映射（CHANNEL_INFO_MAP）、创建/收信分发全部由该
结构自动派生，无需再手动同步多处平行结构。注册顺序即枚举顺序（硬约束，五端一致）。
"""

from dataclasses import dataclass
from typing import Callable, Dict, List, Optional

from .types import EmailInfo, Email, GenerateEmailOptions

from .providers import (
    tempmail,
    tempmail_cn,
    ta_easy,
    linshiyou,
    mffac,
    tempmail_lol,
    chatgpt_org_uk,
    awamail,
    mail_tm,
    dropmail,
    guerrillamail,
    maildrop,
    smail_pw,
    vip_215,
    fake_legal,
    moakt,
    tenminute_one,
    email10min,
    mjj_cm,
    linshi_co,
    harakirimail,
    tempmail_plus,
    tempmail_lol_v2,
    tempgbox,
    emailnator,
    temporam,
    neighbours,
    guerrillamail_mirrors,
    temp_mail_io,
    mail_cx,
    devmail_uk,
    cleantempmail,
    catchmail,
    mailforspam,
    tempmailc,
    tempmail_fish,
    neighbours_sh,
    mailnesia,
    throwawaymail,
    shitty_email,
    tempmailpro,
    m2u,
    tempy_email,
    inboxkitten,
    getnada,
    mail123,
    mail10s,
    webmailtemp,
    tempfastmail,
    one_sec_mail,
    fakemail,
    openinbox,
    inboxes,
    uncorreotemporal,
    zhujump,
    fmail,
    ockito,
    anonbox,
    duckmail,
    mailinator,
    sogetthis_com,
    bobmail_info,
    suremail_info,
    binkmail_com,
    veryrealemail_com,
    chammy_info,
    thisisnotmyrealemail_com,
    notmailinator_com,
    tempmail365,
    tempinbox,
    byom,
    anonymmail,
    eyepaste,
    mail_sunls,
    expressinboxhub,
    lroid,
    haribu,
    rootsh,
    fake_email_site,
    mohmal,
    mailgolem,
    best_temp_mail,
    disposablemail_app,
    mailtemp_cc,
    minuteinbox,
    mailcatch,
    tempemail_co,
    tempemails_net,
    altmails,
    tempemail_info,
    smailpro,
    tempmailten,
    maildrop_cc,
    tenminutemail_net,
    linshiyouxiang_net,
    tempmail_fyi,
    disposablemail_com,
    tempp_mails,
    emailtemp_org,
    mytempmail_cc,
    temp_mail_now,
    mail_td,
    mailhole_de,
    tmail_link,
    chacuo_24mail,
    nimail,
    freecustom,
    apihz,
    mailmomy,
    spamhereplease_com,
    sendspamhere_com,
    sendfree_org,
    junk_beats_org,
    junk_ihmehl_com,
    junk_noplay_org,
    junk_vanillasystem_com,
    spam_jasonpearce_com,
    spam_coroiu_com,
    spam_deluser_net,
    spam_dhsf_net,
    spam_lucatnt_com,
    spam_lyceum_life_com_ru,
    spam_netpirates_net,
    spam_no_ip_net,
    spam_ozh_org,
    spam_pyphus_org,
    spam_shep_pw,
    spam_wtf_at,
    spam_wulczer_org,
    crap_kakadua_net,
    spam_janlugt_nl,
    min_burningfish_net,
    sink_fblay_com,
    etgdev_de,
    mtmdev_com,
    test_unergie_com,
    block_bdea_cc,
    torch_yi_org,
    carlton183_changeip_net,
    mail_fsmash_org,
    ebs_com_ar,
    jama_trenet_eu,
    blackhole_djurby_se,
    m8r_davidfuhr_de,
    mi_meon_be,
    m_nik_me,
    mail_bentrask_com,
    t_zibet_net,
    m8r_mcasal_com,
    ramjane_mooo_com,
    rauxa_seny_cat,
    sp_woot_at,
    fwd2m_eszett_es,
    m_887_at,
    nospam_thurstons_us,
    null_k3vin_net,
    really_istrash_com,
    spam_hortuk_ovh,
    fish_skytale_net,
    spam_mccrew_com,
    dropmail_click,
    n16888888_cyou,
    n17666688_shop,
    n282mail_com,
    bsdu32_buzz,
    doxu243_buzz,
    easyme_pro,
    evergreenco_shop,
    layueming_pics,
    mingyuekeji_online,
    mingyueming_click,
    mingyueming_shop,
    mingyukeji_lol,
    nuxh62_space,
    proid_cloud_ip_cc,
    sbook_pics,
    xue32_buzz,
    b_smelly_cc,
    dea_soon_it,
    disposable_al_sudani_com,
    disposable_nogonad_nl,
    j_fairuse_org,
    mn_curppa_com,
    mailinatorzz_mooo_com,
    notfond_404_mn,
    tempgmailer,
    temp_mail_org,
    xkx_me,
    gonebox_email,
    mailcat_ai,
    tempgo_email,
    restmail_net,
    dropmail_me,
    ten_minute_mail_net,
)


@dataclass
class ChannelSpec:
    """单个渠道的注册规格"""

    channel: str  # 渠道标识
    name: str  # 渠道显示名称
    website: str  # 对应的临时邮箱服务网站
    # 创建邮箱实现：接收 GenerateEmailOptions，返回 EmailInfo
    generate: Callable[[GenerateEmailOptions], EmailInfo]
    # 获取邮件实现：接收 (email, token)，返回 List[Email]
    get_emails: Callable[[str, Optional[str]], List[Email]]


# 有序渠道注册表，注册顺序即枚举顺序（硬约束，五端一致）
CHANNEL_REGISTRY: List[ChannelSpec] = []
# 渠道标识到注册规格的映射，便于 O(1) 查找分发
CHANNEL_REGISTRY_MAP: Dict[str, ChannelSpec] = {}


def register_channel(spec: ChannelSpec) -> None:
    """注册一个渠道：追加到有序列表并建立映射；重复注册视为编程错误"""
    if spec.channel in CHANNEL_REGISTRY_MAP:
        raise ValueError(f"duplicate channel registration: {spec.channel}")
    CHANNEL_REGISTRY.append(spec)
    CHANNEL_REGISTRY_MAP[spec.channel] = spec


def _require_token(token: Optional[str], channel: str) -> str:
    """验证 token 不为空，为空则抛出 ValueError"""
    if not token:
        raise ValueError(f"token is required for {channel} channel")
    return token


def _raise(exc: Exception):
    """在 lambda 表达式中抛出异常的辅助函数（lambda 内无法直接使用 raise 语句）"""
    raise exc


def _with_channel(info: EmailInfo, channel: str) -> EmailInfo:
    """覆写 EmailInfo 的渠道标识（用于同一 provider 派生多个渠道别名）"""
    info.channel = channel
    return info


# 按原 ALL_CHANNELS 顺序注册全部渠道，generate/get_emails 与原两个 dispatch 逐一对应，语义不变
register_channel(
    ChannelSpec(
        channel="tempmail",
        name="TempMail",
        website="tempmail.ing",
        generate=lambda o: tempmail.generate_email(o.duration),
        get_emails=lambda e, t: tempmail.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="tempmail-cn",
        name="TempMail CN",
        website="tempmail.cn",
        generate=lambda o: tempmail_cn.generate_email(o.domain),
        get_emails=lambda e, t: tempmail_cn.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="ta-easy",
        name="TA Easy",
        website="ta-easy.com",
        generate=lambda o: ta_easy.generate_email(),
        get_emails=lambda e, t: ta_easy.get_emails(e, _require_token(t, "ta-easy")),
    )
)
register_channel(
    ChannelSpec(
        channel="10minute-one",
        name="10 Minute Email",
        website="10minutemail.one",
        generate=lambda o: tenminute_one.generate_email(o.domain),
        get_emails=lambda e, t: tenminute_one.get_emails(
            e, _require_token(t, "10minute-one")
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="xghff-com",
        name="xghff.com",
        website="10minutemail.one",
        generate=lambda o: _with_channel(
            tenminute_one.generate_email("xghff.com"), "xghff-com"
        ),
        get_emails=lambda e, t: tenminute_one.get_emails(
            e, _require_token(t, "10minute-one")
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="oqqaj-com",
        name="oqqaj.com",
        website="10minutemail.one",
        generate=lambda o: _with_channel(
            tenminute_one.generate_email("oqqaj.com"), "oqqaj-com"
        ),
        get_emails=lambda e, t: tenminute_one.get_emails(
            e, _require_token(t, "10minute-one")
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="psovv-com",
        name="psovv.com",
        website="10minutemail.one",
        generate=lambda o: _with_channel(
            tenminute_one.generate_email("psovv.com"), "psovv-com"
        ),
        get_emails=lambda e, t: tenminute_one.get_emails(
            e, _require_token(t, "10minute-one")
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="dbwot-com",
        name="dbwot.com",
        website="10minutemail.one",
        generate=lambda o: _with_channel(
            tenminute_one.generate_email("dbwot.com"), "dbwot-com"
        ),
        get_emails=lambda e, t: tenminute_one.get_emails(
            e, _require_token(t, "10minute-one")
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="ygwpr-com",
        name="ygwpr.com",
        website="10minutemail.one",
        generate=lambda o: _with_channel(
            tenminute_one.generate_email("ygwpr.com"), "ygwpr-com"
        ),
        get_emails=lambda e, t: tenminute_one.get_emails(
            e, _require_token(t, "10minute-one")
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="imxwe-com",
        name="imxwe.com",
        website="10minutemail.one",
        generate=lambda o: _with_channel(
            tenminute_one.generate_email("imxwe.com"), "imxwe-com"
        ),
        get_emails=lambda e, t: tenminute_one.get_emails(
            e, _require_token(t, "10minute-one")
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="linshiyou",
        name="临时邮",
        website="linshiyou.com",
        generate=lambda o: linshiyou.generate_email(),
        get_emails=lambda e, t: linshiyou.get_emails(_require_token(t, "linshiyou"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="mffac",
        name="MFFAC",
        website="mffac.com",
        generate=lambda o: mffac.generate_email(),
        get_emails=lambda e, t: mffac.get_emails(e, t or ""),
    )
)
register_channel(
    ChannelSpec(
        channel="tempmail-lol",
        name="TempMail LOL",
        website="tempmail.lol",
        generate=lambda o: tempmail_lol.generate_email(o.domain),
        get_emails=lambda e, t: tempmail_lol.get_emails(
            _require_token(t, "tempmail-lol"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="chatgpt-org-uk",
        name="ChatGPT Mail",
        website="mail.chatgpt.org.uk",
        generate=lambda o: chatgpt_org_uk.generate_email(),
        get_emails=lambda e, t: chatgpt_org_uk.get_emails(
            _require_token(t, "chatgpt-org-uk"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="temp-mail-io",
        name="Temp-Mail.io",
        website="temp-mail.io",
        generate=lambda o: temp_mail_io.generate_email(),
        get_emails=lambda e, t: temp_mail_io.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="mail-cx",
        name="Mail.cx",
        website="mail.cx",
        generate=lambda o: mail_cx.generate_email(o.domain),
        get_emails=lambda e, t: mail_cx.get_emails(_require_token(t, "mail-cx"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="ddker-com",
        name="ddker.com",
        website="mail.cx",
        generate=lambda o: _with_channel(
            mail_cx.generate_email("ddker.com"), "ddker-com"
        ),
        get_emails=lambda e, t: mail_cx.get_emails(_require_token(t, "mail-cx"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="catchmail",
        name="Catchmail",
        website="catchmail.io",
        generate=lambda o: catchmail.generate_email(o.domain),
        get_emails=lambda e, t: catchmail.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="catchmail-mailistry",
        name="Catchmail Mailistry",
        website="mailistry.com",
        generate=lambda o: _with_channel(
            catchmail.generate_email("mailistry.com"), "catchmail-mailistry"
        ),
        get_emails=lambda e, t: catchmail.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="catchmail-zeppost",
        name="Catchmail Zeppost",
        website="zeppost.com",
        generate=lambda o: _with_channel(
            catchmail.generate_email("zeppost.com"), "catchmail-zeppost"
        ),
        get_emails=lambda e, t: catchmail.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="mailforspam",
        name="MailForSpam",
        website="mailforspam.com",
        generate=lambda o: mailforspam.generate_email(o.domain),
        get_emails=lambda e, t: mailforspam.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="mailforspam-tempmail-io",
        name="MailForSpam TempMail.io",
        website="tempmail.io",
        generate=lambda o: _with_channel(
            mailforspam.generate_email("tempmail.io"), "mailforspam-tempmail-io"
        ),
        get_emails=lambda e, t: mailforspam.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="mailforspam-disposable",
        name="MailForSpam Disposable",
        website="disposable.email",
        generate=lambda o: _with_channel(
            mailforspam.generate_email("disposable.email"), "mailforspam-disposable"
        ),
        get_emails=lambda e, t: mailforspam.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="tempmailc",
        name="TempMailC",
        website="tempmailc.com",
        generate=lambda o: tempmailc.generate_email(),
        get_emails=lambda e, t: tempmailc.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="mailnesia",
        name="Mailnesia",
        website="mailnesia.com",
        generate=lambda o: mailnesia.generate_email(),
        get_emails=lambda e, t: mailnesia.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="throwawaymail",
        name="ThrowawayMail",
        website="throwawaymail.app",
        generate=lambda o: throwawaymail.generate_email(),
        get_emails=lambda e, t: throwawaymail.get_emails(
            _require_token(t, "throwawaymail"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="tempmail-fish",
        name="TempMail Fish",
        website="tempmail.fish",
        generate=lambda o: tempmail_fish.generate_email(),
        get_emails=lambda e, t: tempmail_fish.get_emails(
            _require_token(t, "tempmail-fish"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="neighbours-sh",
        name="Neighbours",
        website="neighbours.sh",
        generate=lambda o: neighbours_sh.generate_email(),
        get_emails=lambda e, t: neighbours_sh.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="shitty-email",
        name="shitty.email",
        website="shitty.email",
        generate=lambda o: shitty_email.generate_email(),
        get_emails=lambda e, t: shitty_email.get_emails(
            _require_token(t, "shitty-email"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="tempmailpro",
        name="TempMail Pro",
        website="tempmailpro.us",
        generate=lambda o: tempmailpro.generate_email(),
        get_emails=lambda e, t: tempmailpro.get_emails(
            _require_token(t, "tempmailpro"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="devmail-uk",
        name="DevMail UK",
        website="devmail.uk",
        generate=lambda o: devmail_uk.generate_email(),
        get_emails=lambda e, t: devmail_uk.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="inboxkitten",
        name="InboxKitten",
        website="inboxkitten.com",
        generate=lambda o: inboxkitten.generate_email(),
        get_emails=lambda e, t: inboxkitten.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="cleantempmail",
        name="CleanTempMail",
        website="cleantempmail.com",
        generate=lambda o: cleantempmail.generate_email(),
        get_emails=lambda e, t: cleantempmail.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="getnada",
        name="GetNada",
        website="getnada.net",
        generate=lambda o: getnada.generate_email(o.domain),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="1vpn-net",
        name="1vpn.net",
        website="getnada.net",
        generate=lambda o: getnada.generate_email("1vpn.net", "1vpn-net"),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="abematv-com",
        name="abematv.com",
        website="getnada.net",
        generate=lambda o: getnada.generate_email("abematv.com", "abematv-com"),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="abematv-net",
        name="abematv.net",
        website="getnada.net",
        generate=lambda o: getnada.generate_email("abematv.net", "abematv-net"),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="abematv-org",
        name="abematv.org",
        website="getnada.net",
        generate=lambda o: getnada.generate_email("abematv.org", "abematv-org"),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="aceh-cc",
        name="aceh.cc",
        website="getnada.net",
        generate=lambda o: getnada.generate_email("aceh.cc", "aceh-cc"),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="bangkabelitung-net",
        name="bangkabelitung.net",
        website="getnada.net",
        generate=lambda o: getnada.generate_email(
            "bangkabelitung.net", "bangkabelitung-net"
        ),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="cctruyen-com",
        name="cctruyen.com",
        website="getnada.net",
        generate=lambda o: getnada.generate_email("cctruyen.com", "cctruyen-com"),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="getnada-com",
        name="getnada.com",
        website="getnada.net",
        generate=lambda o: getnada.generate_email("getnada.com", "getnada-com"),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="getnada-email",
        name="getnada.email",
        website="getnada.net",
        generate=lambda o: getnada.generate_email("getnada.email", "getnada-email"),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="getnada-net",
        name="getnada.net",
        website="getnada.net",
        generate=lambda o: getnada.generate_email("getnada.net", "getnada-net"),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="jawatengah-net",
        name="jawatengah.net",
        website="getnada.net",
        generate=lambda o: getnada.generate_email("jawatengah.net", "jawatengah-net"),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="jawatimur-net",
        name="jawatimur.net",
        website="getnada.net",
        generate=lambda o: getnada.generate_email("jawatimur.net", "jawatimur-net"),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="kalimantanbarat-net",
        name="kalimantanbarat.net",
        website="getnada.net",
        generate=lambda o: getnada.generate_email(
            "kalimantanbarat.net", "kalimantanbarat-net"
        ),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="kalimantanselatan-net",
        name="kalimantanselatan.net",
        website="getnada.net",
        generate=lambda o: getnada.generate_email(
            "kalimantanselatan.net", "kalimantanselatan-net"
        ),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="kalimantantengah-net",
        name="kalimantantengah.net",
        website="getnada.net",
        generate=lambda o: getnada.generate_email(
            "kalimantantengah.net", "kalimantantengah-net"
        ),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="kalimantantimur-net",
        name="kalimantantimur.net",
        website="getnada.net",
        generate=lambda o: getnada.generate_email(
            "kalimantantimur.net", "kalimantantimur-net"
        ),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="kalimantanutara-net",
        name="kalimantanutara.net",
        website="getnada.net",
        generate=lambda o: getnada.generate_email(
            "kalimantanutara.net", "kalimantanutara-net"
        ),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="kepulauanriau-net",
        name="kepulauanriau.net",
        website="getnada.net",
        generate=lambda o: getnada.generate_email(
            "kepulauanriau.net", "kepulauanriau-net"
        ),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="luxury345-com",
        name="luxury345.com",
        website="getnada.net",
        generate=lambda o: getnada.generate_email("luxury345.com", "luxury345-com"),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="malukuutara-net",
        name="malukuutara.net",
        website="getnada.net",
        generate=lambda o: getnada.generate_email("malukuutara.net", "malukuutara-net"),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="nusatenggarabarat-net",
        name="nusatenggarabarat.net",
        website="getnada.net",
        generate=lambda o: getnada.generate_email(
            "nusatenggarabarat.net", "nusatenggarabarat-net"
        ),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="nusatenggaratimur-net",
        name="nusatenggaratimur.net",
        website="getnada.net",
        generate=lambda o: getnada.generate_email(
            "nusatenggaratimur.net", "nusatenggaratimur-net"
        ),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="papuabarat-net",
        name="papuabarat.net",
        website="getnada.net",
        generate=lambda o: getnada.generate_email("papuabarat.net", "papuabarat-net"),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="papuabaratdaya-net",
        name="papuabaratdaya.net",
        website="getnada.net",
        generate=lambda o: getnada.generate_email(
            "papuabaratdaya.net", "papuabaratdaya-net"
        ),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="papuaselatan-net",
        name="papuaselatan.net",
        website="getnada.net",
        generate=lambda o: getnada.generate_email(
            "papuaselatan.net", "papuaselatan-net"
        ),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="pehol-com",
        name="pehol.com",
        website="getnada.net",
        generate=lambda o: getnada.generate_email("pehol.com", "pehol-com"),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="ptruyen-com",
        name="ptruyen.com",
        website="getnada.net",
        generate=lambda o: getnada.generate_email("ptruyen.com", "ptruyen-com"),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="pulaubali-net",
        name="pulaubali.net",
        website="getnada.net",
        generate=lambda o: getnada.generate_email("pulaubali.net", "pulaubali-net"),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="riau-net",
        name="riau.net",
        website="getnada.net",
        generate=lambda o: getnada.generate_email("riau.net", "riau-net"),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="seokey-org",
        name="seokey.org",
        website="getnada.net",
        generate=lambda o: getnada.generate_email("seokey.org", "seokey-org"),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="sulawesibarat-net",
        name="sulawesibarat.net",
        website="getnada.net",
        generate=lambda o: getnada.generate_email(
            "sulawesibarat.net", "sulawesibarat-net"
        ),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="sulawesiselatan-net",
        name="sulawesiselatan.net",
        website="getnada.net",
        generate=lambda o: getnada.generate_email(
            "sulawesiselatan.net", "sulawesiselatan-net"
        ),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="sulawesitengah-net",
        name="sulawesitengah.net",
        website="getnada.net",
        generate=lambda o: getnada.generate_email(
            "sulawesitengah.net", "sulawesitengah-net"
        ),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="sulawesitenggara-net",
        name="sulawesitenggara.net",
        website="getnada.net",
        generate=lambda o: getnada.generate_email(
            "sulawesitenggara.net", "sulawesitenggara-net"
        ),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="sumaterabarat-net",
        name="sumaterabarat.net",
        website="getnada.net",
        generate=lambda o: getnada.generate_email(
            "sumaterabarat.net", "sumaterabarat-net"
        ),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="sumateraselatan-net",
        name="sumateraselatan.net",
        website="getnada.net",
        generate=lambda o: getnada.generate_email(
            "sumateraselatan.net", "sumateraselatan-net"
        ),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="sumaterautara-net",
        name="sumaterautara.net",
        website="getnada.net",
        generate=lambda o: getnada.generate_email(
            "sumaterautara.net", "sumaterautara-net"
        ),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="villatogel-com",
        name="villatogel.com",
        website="getnada.net",
        generate=lambda o: getnada.generate_email("villatogel.com", "villatogel-com"),
        get_emails=lambda e, t: getnada.get_emails(_require_token(t, "getnada"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="mail123",
        name="Mail123",
        website="mail123.fr",
        generate=lambda o: mail123.generate_email(),
        get_emails=lambda e, t: mail123.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="mail10s",
        name="Mail10s",
        website="mail10s.com",
        generate=lambda o: mail10s.generate_email(),
        get_emails=lambda e, t: mail10s.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="webmailtemp",
        name="WebMailTemp",
        website="webmailtemp.com",
        generate=lambda o: webmailtemp.generate_email(),
        get_emails=lambda e, t: webmailtemp.get_emails(
            _require_token(t, "webmailtemp"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="tempfastmail",
        name="TempFastMail",
        website="tempfastmail.com",
        generate=lambda o: tempfastmail.generate_email(),
        get_emails=lambda e, t: tempfastmail.get_emails(
            _require_token(t, "tempfastmail"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="1sec-mail",
        name="1SecMail",
        website="1sec-mail.com",
        generate=lambda o: one_sec_mail.generate_email(),
        get_emails=lambda e, t: one_sec_mail.get_emails(
            _require_token(t, "1sec-mail"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="fakemail",
        name="FakeMail",
        website="fakemail.net",
        generate=lambda o: fakemail.generate_email(),
        get_emails=lambda e, t: fakemail.get_emails(_require_token(t, "fakemail"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="openinbox",
        name="OpenInbox",
        website="openinbox.io",
        generate=lambda o: openinbox.generate_email(),
        get_emails=lambda e, t: openinbox.get_emails(_require_token(t, "openinbox"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="inboxes",
        name="Inboxes",
        website="inboxes.com",
        generate=lambda o: inboxes.generate_email(o.domain),
        get_emails=lambda e, t: inboxes.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="uncorreotemporal",
        name="UnCorreoTemporal",
        website="uncorreotemporal.com",
        generate=lambda o: uncorreotemporal.generate_email(),
        get_emails=lambda e, t: uncorreotemporal.get_emails(
            _require_token(t, "uncorreotemporal"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="awamail",
        name="AwaMail",
        website="awamail.com",
        generate=lambda o: awamail.generate_email(),
        get_emails=lambda e, t: awamail.get_emails(_require_token(t, "awamail"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="mail-tm",
        name="Mail.tm",
        website="mail.tm",
        generate=lambda o: mail_tm.generate_email(),
        get_emails=lambda e, t: mail_tm.get_emails(_require_token(t, "mail-tm"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="web-library-net",
        name="web-library.net",
        website="mail.tm",
        generate=lambda o: _with_channel(mail_tm.generate_email(), "web-library-net"),
        get_emails=lambda e, t: mail_tm.get_emails(_require_token(t, "mail-tm"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="dropmail",
        name="DropMail",
        website="dropmail.me",
        generate=lambda o: dropmail.generate_email(),
        get_emails=lambda e, t: dropmail.get_emails(_require_token(t, "dropmail"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="guerrillamail",
        name="Guerrilla Mail",
        website="guerrillamail.com",
        generate=lambda o: guerrillamail.generate_email(),
        get_emails=lambda e, t: guerrillamail.get_emails(
            _require_token(t, "guerrillamail"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="guerrillamail-com",
        name="GuerrillaMail Root",
        website="guerrillamail.com",
        generate=lambda o: guerrillamail_mirrors.guerrillamail_com_generate(),
        get_emails=lambda e, t: guerrillamail_mirrors.guerrillamail_com_get_emails(
            _require_token(t, "guerrillamail-com"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="maildrop",
        name="Maildrop",
        website="maildrop.cx",
        generate=lambda o: maildrop.generate_email(o.domain),
        get_emails=lambda e, t: maildrop.get_emails(_require_token(t, "maildrop"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="smail-pw",
        name="Smail.pw",
        website="smail.pw",
        generate=lambda o: smail_pw.generate_email(),
        get_emails=lambda e, t: smail_pw.get_emails(_require_token(t, "smail-pw"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="vip-215",
        name="VIP 215",
        website="vip.215.im",
        generate=lambda o: vip_215.generate_email(),
        get_emails=lambda e, t: vip_215.get_emails(_require_token(t, "vip-215"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="fake-legal",
        name="Fake Legal",
        website="fake.legal",
        generate=lambda o: fake_legal.generate_email(o.domain),
        get_emails=lambda e, t: fake_legal.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="imgui-de",
        name="imgui.de",
        website="fake.legal",
        generate=lambda o: fake_legal.generate_email("imgui.de", "imgui-de"),
        get_emails=lambda e, t: fake_legal.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="pulsewebmenu-de",
        name="pulsewebmenu.de",
        website="fake.legal",
        generate=lambda o: fake_legal.generate_email(
            "pulsewebmenu.de", "pulsewebmenu-de"
        ),
        get_emails=lambda e, t: fake_legal.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="moakt",
        name="Moakt",
        website="moakt.com",
        generate=lambda o: moakt.generate_email(o.domain),
        get_emails=lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    )
)
register_channel(
    ChannelSpec(
        channel="drmail-in",
        name="drmail.in",
        website="moakt.com",
        generate=lambda o: _with_channel(
            moakt.generate_email("drmail.in"), "drmail-in"
        ),
        get_emails=lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    )
)
register_channel(
    ChannelSpec(
        channel="teml-net",
        name="teml.net",
        website="moakt.com",
        generate=lambda o: _with_channel(moakt.generate_email("teml.net"), "teml-net"),
        get_emails=lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    )
)
register_channel(
    ChannelSpec(
        channel="tmpeml-com",
        name="tmpeml.com",
        website="moakt.com",
        generate=lambda o: _with_channel(
            moakt.generate_email("tmpeml.com"), "tmpeml-com"
        ),
        get_emails=lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    )
)
register_channel(
    ChannelSpec(
        channel="tmpbox-net",
        name="tmpbox.net",
        website="moakt.com",
        generate=lambda o: _with_channel(
            moakt.generate_email("tmpbox.net"), "tmpbox-net"
        ),
        get_emails=lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    )
)
register_channel(
    ChannelSpec(
        channel="moakt-cc",
        name="moakt.cc",
        website="moakt.com",
        generate=lambda o: _with_channel(moakt.generate_email("moakt.cc"), "moakt-cc"),
        get_emails=lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    )
)
register_channel(
    ChannelSpec(
        channel="disbox-net",
        name="disbox.net",
        website="moakt.com",
        generate=lambda o: _with_channel(
            moakt.generate_email("disbox.net"), "disbox-net"
        ),
        get_emails=lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    )
)
register_channel(
    ChannelSpec(
        channel="tmpmail-org",
        name="tmpmail.org",
        website="moakt.com",
        generate=lambda o: _with_channel(
            moakt.generate_email("tmpmail.org"), "tmpmail-org"
        ),
        get_emails=lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    )
)
register_channel(
    ChannelSpec(
        channel="tmpmail-net",
        name="tmpmail.net",
        website="moakt.com",
        generate=lambda o: _with_channel(
            moakt.generate_email("tmpmail.net"), "tmpmail-net"
        ),
        get_emails=lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    )
)
register_channel(
    ChannelSpec(
        channel="tmails-net",
        name="tmails.net",
        website="moakt.com",
        generate=lambda o: _with_channel(
            moakt.generate_email("tmails.net"), "tmails-net"
        ),
        get_emails=lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    )
)
register_channel(
    ChannelSpec(
        channel="disbox-org",
        name="disbox.org",
        website="moakt.com",
        generate=lambda o: _with_channel(
            moakt.generate_email("disbox.org"), "disbox-org"
        ),
        get_emails=lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    )
)
register_channel(
    ChannelSpec(
        channel="moakt-co",
        name="moakt.co",
        website="moakt.com",
        generate=lambda o: _with_channel(moakt.generate_email("moakt.co"), "moakt-co"),
        get_emails=lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    )
)
register_channel(
    ChannelSpec(
        channel="moakt-ws",
        name="moakt.ws",
        website="moakt.com",
        generate=lambda o: _with_channel(moakt.generate_email("moakt.ws"), "moakt-ws"),
        get_emails=lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    )
)
register_channel(
    ChannelSpec(
        channel="tmail-ws",
        name="tmail.ws",
        website="moakt.com",
        generate=lambda o: _with_channel(moakt.generate_email("tmail.ws"), "tmail-ws"),
        get_emails=lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    )
)
register_channel(
    ChannelSpec(
        channel="bareed-ws",
        name="bareed.ws",
        website="moakt.com",
        generate=lambda o: _with_channel(
            moakt.generate_email("bareed.ws"), "bareed-ws"
        ),
        get_emails=lambda e, t: moakt.get_emails(e, _require_token(t, "moakt")),
    )
)
register_channel(
    ChannelSpec(
        channel="email10min",
        name="Email10Min",
        website="email10min.com",
        generate=lambda o: email10min.generate_email(),
        get_emails=lambda e, t: email10min.get_emails(
            e, _require_token(t, "email10min")
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="mjj-cm",
        name="MJJ Mail",
        website="mjj.cm",
        generate=lambda o: mjj_cm.generate_email(),
        get_emails=lambda e, t: mjj_cm.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="linshi-co",
        name="临时Co",
        website="linshi.co",
        generate=lambda o: linshi_co.generate_email(),
        get_emails=lambda e, t: linshi_co.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="harakirimail",
        name="HarakiriMail",
        website="harakirimail.com",
        generate=lambda o: harakirimail.generate_email(),
        get_emails=lambda e, t: harakirimail.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="jqkjqk-xyz",
        name="jqkjqk.xyz",
        website="mail.zhujump.com",
        generate=lambda o: zhujump.generate_email("jqkjqk.xyz", "jqkjqk-xyz"),
        get_emails=lambda e, t: zhujump.get_emails(_require_token(t, "jqkjqk-xyz"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="lyhlevi-com",
        name="LyhLevi MoeMail",
        website="lyhlevi.com",
        generate=lambda o: zhujump.generate_email_for_instance(
            "https://lyhlevi.com", "lyhlevi.com", "lyhlevi-com", 24 * 60 * 60 * 1000
        ),
        get_emails=lambda e, t: zhujump.get_emails(_require_token(t, "lyhlevi-com"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="tempmail-plus",
        name="TempMail Plus",
        website="tempmail.plus",
        generate=lambda o: tempmail_plus.generate_email(),
        get_emails=lambda e, t: tempmail_plus.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="fexpost-com",
        name="fexpost.com",
        website="tempmail.plus",
        generate=lambda o: tempmail_plus.generate_email("fexpost.com", "fexpost-com"),
        get_emails=lambda e, t: tempmail_plus.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="fexbox-org",
        name="fexbox.org",
        website="tempmail.plus",
        generate=lambda o: tempmail_plus.generate_email("fexbox.org", "fexbox-org"),
        get_emails=lambda e, t: tempmail_plus.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="mailbox-in-ua",
        name="mailbox.in.ua",
        website="tempmail.plus",
        generate=lambda o: tempmail_plus.generate_email(
            "mailbox.in.ua", "mailbox-in-ua"
        ),
        get_emails=lambda e, t: tempmail_plus.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="rover-info",
        name="rover.info",
        website="tempmail.plus",
        generate=lambda o: tempmail_plus.generate_email("rover.info", "rover-info"),
        get_emails=lambda e, t: tempmail_plus.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="chitthi-in",
        name="chitthi.in",
        website="tempmail.plus",
        generate=lambda o: tempmail_plus.generate_email("chitthi.in", "chitthi-in"),
        get_emails=lambda e, t: tempmail_plus.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="fextemp-com",
        name="fextemp.com",
        website="tempmail.plus",
        generate=lambda o: tempmail_plus.generate_email("fextemp.com", "fextemp-com"),
        get_emails=lambda e, t: tempmail_plus.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="any-pink",
        name="any.pink",
        website="tempmail.plus",
        generate=lambda o: tempmail_plus.generate_email("any.pink", "any-pink"),
        get_emails=lambda e, t: tempmail_plus.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="merepost-com",
        name="merepost.com",
        website="tempmail.plus",
        generate=lambda o: tempmail_plus.generate_email("merepost.com", "merepost-com"),
        get_emails=lambda e, t: tempmail_plus.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="tempmail-lol-v2",
        name="TempMail LOL V2",
        website="tempmail.lol",
        generate=lambda o: tempmail_lol_v2.generate_email(),
        get_emails=lambda e, t: tempmail_lol_v2.get_emails(
            _require_token(t, "tempmail-lol-v2"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="tempgbox",
        name="TempGBox",
        website="tempgbox.net",
        generate=lambda o: tempgbox.generate_email(),
        get_emails=lambda e, t: tempgbox.get_emails(_require_token(t, "tempgbox"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="emailnator",
        name="Emailnator",
        website="emailnator.com",
        generate=lambda o: emailnator.generate_email(),
        get_emails=lambda e, t: emailnator.get_emails(
            _require_token(t, "emailnator"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="temporam",
        name="Temporam",
        website="temporam.com",
        generate=lambda o: temporam.generate_email(o.domain),
        get_emails=lambda e, t: temporam.get_emails(t, e),
    )
)
register_channel(
    ChannelSpec(
        channel="neighbours",
        name="Neighbours",
        website="neighbours.sh",
        generate=lambda o: neighbours.generate_email(o.domain),
        get_emails=lambda e, t: neighbours.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="sharklasers",
        name="SharkLasers",
        website="sharklasers.com",
        generate=lambda o: guerrillamail_mirrors.sharklasers_generate(),
        get_emails=lambda e, t: guerrillamail_mirrors.sharklasers_get_emails(
            _require_token(t, "sharklasers"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="sharklasers-com",
        name="SharkLasers Root",
        website="sharklasers.com",
        generate=lambda o: guerrillamail_mirrors.sharklasers_com_generate(),
        get_emails=lambda e, t: guerrillamail_mirrors.sharklasers_com_get_emails(
            _require_token(t, "sharklasers-com"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="grr-la",
        name="Grr.la",
        website="grr.la",
        generate=lambda o: guerrillamail_mirrors.grrla_generate(),
        get_emails=lambda e, t: guerrillamail_mirrors.grrla_get_emails(
            _require_token(t, "grr-la"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="grr-la-com",
        name="Grr.la Root",
        website="grr.la",
        generate=lambda o: guerrillamail_mirrors.grrla_com_generate(),
        get_emails=lambda e, t: guerrillamail_mirrors.grrla_com_get_emails(
            _require_token(t, "grr-la-com"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="guerrillamail-info",
        name="GuerrillaMail Info",
        website="guerrillamail.info",
        generate=lambda o: guerrillamail_mirrors.guerrillamail_info_generate(),
        get_emails=lambda e, t: guerrillamail_mirrors.guerrillamail_info_get_emails(
            _require_token(t, "guerrillamail-info"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="spam4me",
        name="Spam4.me",
        website="spam4.me",
        generate=lambda o: guerrillamail_mirrors.spam4me_generate(),
        get_emails=lambda e, t: guerrillamail_mirrors.spam4me_get_emails(
            _require_token(t, "spam4me"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="guerrillamail-net",
        name="GuerrillaMail Net",
        website="guerrillamail.net",
        generate=lambda o: guerrillamail_mirrors.guerrillamail_net_generate(),
        get_emails=lambda e, t: guerrillamail_mirrors.guerrillamail_net_get_emails(
            _require_token(t, "guerrillamail-net"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="guerrillamail-org",
        name="GuerrillaMail Org",
        website="guerrillamail.org",
        generate=lambda o: guerrillamail_mirrors.guerrillamail_org_generate(),
        get_emails=lambda e, t: guerrillamail_mirrors.guerrillamail_org_get_emails(
            _require_token(t, "guerrillamail-org"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="guerrillamailblock",
        name="GuerrillaMailBlock",
        website="guerrillamailblock.com",
        generate=lambda o: guerrillamail_mirrors.guerrillamailblock_generate(),
        get_emails=lambda e, t: guerrillamail_mirrors.guerrillamailblock_get_emails(
            _require_token(t, "guerrillamailblock"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="guerrillamail-com-www",
        name="GuerrillaMail WWW",
        website="guerrillamail.com",
        generate=lambda o: guerrillamail_mirrors.guerrillamail_com_www_generate(),
        get_emails=lambda e, t: guerrillamail_mirrors.guerrillamail_com_www_get_emails(
            _require_token(t, "guerrillamail-com-www"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="m2u",
        name="MailToYou",
        website="m2u.io",
        generate=lambda o: m2u.generate_email(),
        get_emails=lambda e, t: m2u.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="tempy-email",
        name="Tempy Email",
        website="tempy.email",
        generate=lambda o: tempy_email.generate_email(o.domain),
        get_emails=lambda e, t: tempy_email.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="fmail",
        name="FMail",
        website="fmail.men",
        generate=lambda o: fmail.generate_email(o.domain),
        get_emails=lambda e, t: fmail.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="ockito",
        name="Ockito",
        website="ockito.com",
        generate=lambda o: ockito.generate_email(),
        get_emails=lambda e, t: ockito.get_emails(
            t if t else _raise(ValueError("internal error: token missing for ockito")),
            e,
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="anonbox",
        name="Anonbox",
        website="anonbox.net",
        generate=lambda o: anonbox.generate_email(),
        get_emails=lambda e, t: anonbox.get_emails(_require_token(t, "anonbox"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="duckmail",
        name="DuckMail",
        website="duckmail.sbs",
        generate=lambda o: duckmail.generate_email(),
        get_emails=lambda e, t: duckmail.get_emails(_require_token(t, "duckmail"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="mailinator",
        name="Mailinator",
        website="mailinator.com",
        generate=lambda o: mailinator.generate_email(),
        get_emails=lambda e, t: mailinator.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="tempmail365",
        name="Tempmail365",
        website="tempmail365.cn",
        generate=lambda o: tempmail365.generate_email(o.domain),
        get_emails=lambda e, t: tempmail365.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="tempinbox",
        name="TempInbox",
        website="www.tempinbox.xyz",
        generate=lambda o: tempinbox.generate_email(o.domain),
        get_emails=lambda e, t: tempinbox.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="byom",
        name="Byom",
        website="byom.de",
        generate=lambda o: byom.generate_email(),
        get_emails=lambda e, t: byom.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="anonymmail",
        name="Anonymmail",
        website="anonymmail.net",
        generate=lambda o: anonymmail.generate_email(),
        get_emails=lambda e, t: anonymmail.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="eyepaste",
        name="Eyepaste",
        website="eyepaste.com",
        generate=lambda o: eyepaste.generate_email(),
        get_emails=lambda e, t: eyepaste.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="mail-sunls",
        name="Mail Sunls",
        website="mail.sunls.de",
        generate=lambda o: mail_sunls.generate_email(),
        get_emails=lambda e, t: mail_sunls.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="expressinboxhub",
        name="ExpressInboxHub",
        website="expressinboxhub.com",
        generate=lambda o: expressinboxhub.generate_email(),
        get_emails=lambda e, t: expressinboxhub.get_emails(
            _require_token(t, "expressinboxhub"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="lroid",
        name="Lroid",
        website="lroid.com",
        generate=lambda o: lroid.generate_email(),
        get_emails=lambda e, t: lroid.get_emails(_require_token(t, "lroid"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="haribu",
        name="Haribu",
        website="haribu.net",
        generate=lambda o: haribu.generate_email(),
        get_emails=lambda e, t: haribu.get_emails(_require_token(t, "haribu"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="rootsh",
        name="Rootsh(BccTo)",
        website="rootsh.com",
        generate=lambda o: rootsh.generate_email(o.domain),
        get_emails=lambda e, t: rootsh.get_emails(_require_token(t, "rootsh"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="fake-email-site",
        name="FakeEmailSite",
        website="fake-email.site",
        generate=lambda o: fake_email_site.generate_email(),
        get_emails=lambda e, t: fake_email_site.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="mohmal",
        name="Mohmal",
        website="mohmal.com",
        generate=lambda o: mohmal.generate_email(),
        get_emails=lambda e, t: mohmal.get_emails(e, _require_token(t, "mohmal")),
    )
)
register_channel(
    ChannelSpec(
        channel="mailgolem",
        name="MailGolem",
        website="mailgolem.com",
        generate=lambda o: mailgolem.generate_email(),
        get_emails=lambda e, t: mailgolem.get_emails(_require_token(t, "mailgolem"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="best-temp-mail",
        name="BestTempMail",
        website="best-temp-mail.com",
        generate=lambda o: best_temp_mail.generate_email(),
        get_emails=lambda e, t: best_temp_mail.get_emails(
            _require_token(t, "best-temp-mail"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="disposablemail-app",
        name="DisposableMail",
        website="disposablemail.app",
        generate=lambda o: disposablemail_app.generate_email(),
        get_emails=lambda e, t: disposablemail_app.get_emails(
            e, _require_token(t, "disposablemail-app")
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="mailtemp-cc",
        name="MailTemp.cc",
        website="mailtemp.cc",
        generate=lambda o: mailtemp_cc.generate_email(),
        get_emails=lambda e, t: mailtemp_cc.get_emails(
            e, _require_token(t, "mailtemp-cc")
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="minuteinbox",
        name="MinuteInbox",
        website="minuteinbox.com",
        generate=lambda o: minuteinbox.generate_email(),
        get_emails=lambda e, t: minuteinbox.get_emails(
            e, _require_token(t, "minuteinbox")
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="mailcatch",
        name="MailCatch",
        website="mailcatch.com",
        generate=lambda o: mailcatch.generate_email(),
        get_emails=lambda e, t: mailcatch.get_emails(e, _require_token(t, "mailcatch")),
    )
)
register_channel(
    ChannelSpec(
        channel="tempemail-co",
        name="TempEmail.co",
        website="tempemail.co",
        generate=lambda o: tempemail_co.generate_email(),
        get_emails=lambda e, t: tempemail_co.get_emails(
            e, _require_token(t, "tempemail-co")
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="tempemails-net",
        name="TempEmails.net",
        website="tempemails.net",
        generate=lambda o: tempemails_net.generate_email(),
        get_emails=lambda e, t: tempemails_net.get_emails(
            e, _require_token(t, "tempemails-net")
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="altmails",
        name="AltMails",
        website="tempmail.altmails.com",
        generate=lambda o: altmails.generate_email(),
        get_emails=lambda e, t: altmails.get_emails(e, _require_token(t, "altmails")),
    )
)
register_channel(
    ChannelSpec(
        channel="tempemail-info",
        name="TempEmailInfo",
        website="tempemail.info",
        generate=lambda o: tempemail_info.generate_email(),
        get_emails=lambda e, t: tempemail_info.get_emails(
            e, _require_token(t, "tempemail-info")
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="smailpro",
        name="SmailPro",
        website="smailpro.com",
        generate=lambda o: smailpro.generate_email(),
        get_emails=lambda e, t: smailpro.get_emails(e, t or ""),
    )
)
register_channel(
    ChannelSpec(
        channel="tempmailten",
        name="TempMailTen",
        website="tempmailten.com",
        generate=lambda o: tempmailten.generate_email(),
        get_emails=lambda e, t: tempmailten.get_emails(
            e, _require_token(t, "tempmailten")
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="maildrop-cc",
        name="MailDrop.cc",
        website="maildrop.cc",
        generate=lambda o: maildrop_cc.generate_email(),
        get_emails=lambda e, t: maildrop_cc.get_emails(e, t or ""),
    )
)
register_channel(
    ChannelSpec(
        channel="10minutemail-net",
        name="10MinuteMail.net",
        website="10minutemail.net",
        generate=lambda o: tenminutemail_net.generate_email(),
        get_emails=lambda e, t: tenminutemail_net.get_emails(
            e, _require_token(t, "10minutemail-net")
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="linshiyouxiang-net",
        name="LinShiYouXiang",
        website="linshiyouxiang.net",
        generate=lambda o: linshiyouxiang_net.generate_email(),
        get_emails=lambda e, t: linshiyouxiang_net.get_emails(
            e, _require_token(t, "linshiyouxiang-net")
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="tempmail-fyi",
        name="TempMail.FYI",
        website="temp-mail.fyi",
        generate=lambda o: tempmail_fyi.generate_email(),
        get_emails=lambda e, t: tempmail_fyi.get_emails(
            e, _require_token(t, "tempmail-fyi")
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="disposablemail-com",
        name="DisposableMail",
        website="disposablemail.com",
        generate=lambda o: disposablemail_com.generate_email(),
        get_emails=lambda e, t: disposablemail_com.get_emails(
            e, _require_token(t, "disposablemail-com")
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="tempp-mails",
        name="TemppMails",
        website="tempp-mails.com",
        generate=lambda o: tempp_mails.generate_email(),
        get_emails=lambda e, t: tempp_mails.get_emails(
            e, _require_token(t, "tempp-mails")
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="emailtemp-org",
        name="EmailTemp",
        website="emailtemp.org",
        generate=lambda o: emailtemp_org.generate_email(),
        get_emails=lambda e, t: emailtemp_org.get_emails(
            e, _require_token(t, "emailtemp-org")
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="mytempmail-cc",
        name="MyTempMail.cc",
        website="mytempmail.cc",
        generate=lambda o: mytempmail_cc.generate_email(),
        get_emails=lambda e, t: mytempmail_cc.get_emails(
            _require_token(t, "mytempmail-cc"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="temp-mail-now",
        name="TempMailNow",
        website="temp-mail.now",
        generate=lambda o: temp_mail_now.generate_email(),
        get_emails=lambda e, t: temp_mail_now.get_emails(
            _require_token(t, "temp-mail-now"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="mail-td",
        name="Mail.TD",
        website="mail.td",
        generate=lambda o: mail_td.generate_email(),
        get_emails=lambda e, t: mail_td.get_emails(_require_token(t, "mail-td"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="mailhole-de",
        name="Mailhole.de",
        website="mailhole.de",
        generate=lambda o: mailhole_de.generate_email(),
        get_emails=lambda e, t: mailhole_de.get_emails(t or e),
    )
)
register_channel(
    ChannelSpec(
        channel="tmail-link",
        name="TMail.Link",
        website="tmail.link",
        generate=lambda o: tmail_link.generate_email(),
        get_emails=lambda e, t: tmail_link.get_emails(
            e, _require_token(t, "tmail-link")
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="24mail-chacuo",
        name="24Mail Chacuo",
        website="24mail.chacuo.net",
        generate=lambda o: chacuo_24mail.generate_email(o.domain),
        get_emails=lambda e, t: chacuo_24mail.get_emails(
            e, _require_token(t, "24mail-chacuo")
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="nimail",
        name="NiMail",
        website="nimail.cn",
        generate=lambda o: nimail.generate_email(),
        get_emails=lambda e, t: nimail.get_emails(t or e),
    )
)
register_channel(
    ChannelSpec(
        channel="freecustom",
        name="FreeCustom.Email",
        website="freecustom.email",
        generate=lambda o: freecustom.generate_email(),
        get_emails=lambda e, t: freecustom.get_emails(t or e),
    )
)
register_channel(
    ChannelSpec(
        channel="16888888-cyou",
        name="Mailmomy (16888888.cyou)",
        website="mailmomy.com",
        generate=lambda o: n16888888_cyou.generate_email(),
        get_emails=lambda e, t: n16888888_cyou.get_emails(t or e),
    )
)
register_channel(
    ChannelSpec(
        channel="17666688-shop",
        name="Mailmomy (17666688.shop)",
        website="mailmomy.com",
        generate=lambda o: n17666688_shop.generate_email(),
        get_emails=lambda e, t: n17666688_shop.get_emails(t or e),
    )
)
register_channel(
    ChannelSpec(
        channel="282mail-com",
        name="Mailmomy (282mail.com)",
        website="mailmomy.com",
        generate=lambda o: n282mail_com.generate_email(),
        get_emails=lambda e, t: n282mail_com.get_emails(t or e),
    )
)
register_channel(
    ChannelSpec(
        channel="blackhole-djurby-se",
        name="Mailinator (blackhole.djurby.se)",
        website="mailinator.com",
        generate=lambda o: blackhole_djurby_se.generate_email(),
        get_emails=lambda e, t: blackhole_djurby_se.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="block-bdea-cc",
        name="Mailinator (block.bdea.cc)",
        website="mailinator.com",
        generate=lambda o: block_bdea_cc.generate_email(),
        get_emails=lambda e, t: block_bdea_cc.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="bsdu32-buzz",
        name="Mailmomy (bsdu32.buzz)",
        website="mailmomy.com",
        generate=lambda o: bsdu32_buzz.generate_email(),
        get_emails=lambda e, t: bsdu32_buzz.get_emails(t or e),
    )
)
register_channel(
    ChannelSpec(
        channel="b-smelly-cc",
        name="Mailinator (b.smelly.cc)",
        website="mailinator.com",
        generate=lambda o: b_smelly_cc.generate_email(),
        get_emails=lambda e, t: b_smelly_cc.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="carlton183-changeip-net",
        name="Mailinator (183carlton.changeip.net)",
        website="mailinator.com",
        generate=lambda o: carlton183_changeip_net.generate_email(),
        get_emails=lambda e, t: carlton183_changeip_net.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="dea-soon-it",
        name="Mailinator (dea.soon.it)",
        website="mailinator.com",
        generate=lambda o: dea_soon_it.generate_email(),
        get_emails=lambda e, t: dea_soon_it.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="disposable-al-sudani-com",
        name="Mailinator (disposable.al-sudani.com)",
        website="mailinator.com",
        generate=lambda o: disposable_al_sudani_com.generate_email(),
        get_emails=lambda e, t: disposable_al_sudani_com.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="disposable-nogonad-nl",
        name="Mailinator (disposable.nogonad.nl)",
        website="mailinator.com",
        generate=lambda o: disposable_nogonad_nl.generate_email(),
        get_emails=lambda e, t: disposable_nogonad_nl.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="doxu243-buzz",
        name="Mailmomy (doxu243.buzz)",
        website="mailmomy.com",
        generate=lambda o: doxu243_buzz.generate_email(),
        get_emails=lambda e, t: doxu243_buzz.get_emails(t or e),
    )
)
register_channel(
    ChannelSpec(
        channel="easyme-pro",
        name="Mailmomy (easyme.pro)",
        website="mailmomy.com",
        generate=lambda o: easyme_pro.generate_email(),
        get_emails=lambda e, t: easyme_pro.get_emails(t or e),
    )
)
register_channel(
    ChannelSpec(
        channel="ebs-com-ar",
        name="Mailinator (ebs.com.ar)",
        website="mailinator.com",
        generate=lambda o: ebs_com_ar.generate_email(),
        get_emails=lambda e, t: ebs_com_ar.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="etgdev-de",
        name="Mailinator (etgdev.de)",
        website="mailinator.com",
        generate=lambda o: etgdev_de.generate_email(),
        get_emails=lambda e, t: etgdev_de.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="evergreenco-shop",
        name="Mailmomy (evergreenco.shop)",
        website="mailmomy.com",
        generate=lambda o: evergreenco_shop.generate_email(),
        get_emails=lambda e, t: evergreenco_shop.get_emails(t or e),
    )
)
register_channel(
    ChannelSpec(
        channel="fwd2m-eszett-es",
        name="Mailinator (fwd2m.eszett.es)",
        website="mailinator.com",
        generate=lambda o: fwd2m_eszett_es.generate_email(),
        get_emails=lambda e, t: fwd2m_eszett_es.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="jama-trenet-eu",
        name="Mailinator (jama.trenet.eu)",
        website="mailinator.com",
        generate=lambda o: jama_trenet_eu.generate_email(),
        get_emails=lambda e, t: jama_trenet_eu.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="j-fairuse-org",
        name="Mailinator (j.fairuse.org)",
        website="mailinator.com",
        generate=lambda o: j_fairuse_org.generate_email(),
        get_emails=lambda e, t: j_fairuse_org.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="layueming-pics",
        name="Mailmomy (layueming.pics)",
        website="mailmomy.com",
        generate=lambda o: layueming_pics.generate_email(),
        get_emails=lambda e, t: layueming_pics.get_emails(t or e),
    )
)
register_channel(
    ChannelSpec(
        channel="m-887-at",
        name="Mailinator (m.887.at)",
        website="mailinator.com",
        generate=lambda o: m_887_at.generate_email(),
        get_emails=lambda e, t: m_887_at.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="m8r-davidfuhr-de",
        name="Mailinator (m8r.davidfuhr.de)",
        website="mailinator.com",
        generate=lambda o: m8r_davidfuhr_de.generate_email(),
        get_emails=lambda e, t: m8r_davidfuhr_de.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="m8r-mcasal-com",
        name="Mailinator (m8r.mcasal.com)",
        website="mailinator.com",
        generate=lambda o: m8r_mcasal_com.generate_email(),
        get_emails=lambda e, t: m8r_mcasal_com.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="mail-bentrask-com",
        name="Mailinator (mail.bentrask.com)",
        website="mailinator.com",
        generate=lambda o: mail_bentrask_com.generate_email(),
        get_emails=lambda e, t: mail_bentrask_com.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="mail-fsmash-org",
        name="Mailinator (mail.fsmash.org)",
        website="mailinator.com",
        generate=lambda o: mail_fsmash_org.generate_email(),
        get_emails=lambda e, t: mail_fsmash_org.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="mailinatorzz-mooo-com",
        name="Mailinator (mailinatorzz.mooo.com)",
        website="mailinator.com",
        generate=lambda o: mailinatorzz_mooo_com.generate_email(),
        get_emails=lambda e, t: mailinatorzz_mooo_com.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="mi-meon-be",
        name="Mailinator (mi.meon.be)",
        website="mailinator.com",
        generate=lambda o: mi_meon_be.generate_email(),
        get_emails=lambda e, t: mi_meon_be.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="mingyuekeji-online",
        name="Mailmomy (mingyuekeji.online)",
        website="mailmomy.com",
        generate=lambda o: mingyuekeji_online.generate_email(),
        get_emails=lambda e, t: mingyuekeji_online.get_emails(t or e),
    )
)
register_channel(
    ChannelSpec(
        channel="mingyueming-click",
        name="Mailmomy (mingyueming.click)",
        website="mailmomy.com",
        generate=lambda o: mingyueming_click.generate_email(),
        get_emails=lambda e, t: mingyueming_click.get_emails(t or e),
    )
)
register_channel(
    ChannelSpec(
        channel="mingyueming-shop",
        name="Mailmomy (mingyueming.shop)",
        website="mailmomy.com",
        generate=lambda o: mingyueming_shop.generate_email(),
        get_emails=lambda e, t: mingyueming_shop.get_emails(t or e),
    )
)
register_channel(
    ChannelSpec(
        channel="mingyukeji-lol",
        name="Mailmomy (mingyukeji.lol)",
        website="mailmomy.com",
        generate=lambda o: mingyukeji_lol.generate_email(),
        get_emails=lambda e, t: mingyukeji_lol.get_emails(t or e),
    )
)
register_channel(
    ChannelSpec(
        channel="mn-curppa-com",
        name="Mailinator (mn.curppa.com)",
        website="mailinator.com",
        generate=lambda o: mn_curppa_com.generate_email(),
        get_emails=lambda e, t: mn_curppa_com.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="m-nik-me",
        name="Mailinator (m.nik.me)",
        website="mailinator.com",
        generate=lambda o: m_nik_me.generate_email(),
        get_emails=lambda e, t: m_nik_me.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="mtmdev-com",
        name="Mailinator (mtmdev.com)",
        website="mailinator.com",
        generate=lambda o: mtmdev_com.generate_email(),
        get_emails=lambda e, t: mtmdev_com.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="nospam-thurstons-us",
        name="Mailinator (nospam.thurstons.us)",
        website="mailinator.com",
        generate=lambda o: nospam_thurstons_us.generate_email(),
        get_emails=lambda e, t: nospam_thurstons_us.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="notfond-404-mn",
        name="Mailinator (notfond.404.mn)",
        website="mailinator.com",
        generate=lambda o: notfond_404_mn.generate_email(),
        get_emails=lambda e, t: notfond_404_mn.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="null-k3vin-net",
        name="Mailinator (null.k3vin.net)",
        website="mailinator.com",
        generate=lambda o: null_k3vin_net.generate_email(),
        get_emails=lambda e, t: null_k3vin_net.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="nuxh62-space",
        name="Mailmomy (nuxh62.space)",
        website="mailmomy.com",
        generate=lambda o: nuxh62_space.generate_email(),
        get_emails=lambda e, t: nuxh62_space.get_emails(t or e),
    )
)
register_channel(
    ChannelSpec(
        channel="proid-cloud-ip-cc",
        name="Mailmomy (proid.cloud-ip.cc)",
        website="mailmomy.com",
        generate=lambda o: proid_cloud_ip_cc.generate_email(),
        get_emails=lambda e, t: proid_cloud_ip_cc.get_emails(t or e),
    )
)
register_channel(
    ChannelSpec(
        channel="ramjane-mooo-com",
        name="Mailinator (ramjane.mooo.com)",
        website="mailinator.com",
        generate=lambda o: ramjane_mooo_com.generate_email(),
        get_emails=lambda e, t: ramjane_mooo_com.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="rauxa-seny-cat",
        name="Mailinator (rauxa.seny.cat)",
        website="mailinator.com",
        generate=lambda o: rauxa_seny_cat.generate_email(),
        get_emails=lambda e, t: rauxa_seny_cat.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="really-istrash-com",
        name="Mailinator (really.istrash.com)",
        website="mailinator.com",
        generate=lambda o: really_istrash_com.generate_email(),
        get_emails=lambda e, t: really_istrash_com.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="sbook-pics",
        name="Mailmomy (sbook.pics)",
        website="mailmomy.com",
        generate=lambda o: sbook_pics.generate_email(),
        get_emails=lambda e, t: sbook_pics.get_emails(t or e),
    )
)
register_channel(
    ChannelSpec(
        channel="spam-hortuk-ovh",
        name="Mailinator (spam.hortuk.ovh)",
        website="mailinator.com",
        generate=lambda o: spam_hortuk_ovh.generate_email(),
        get_emails=lambda e, t: spam_hortuk_ovh.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="sp-woot-at",
        name="Mailinator (sp.woot.at)",
        website="mailinator.com",
        generate=lambda o: sp_woot_at.generate_email(),
        get_emails=lambda e, t: sp_woot_at.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="test-unergie-com",
        name="Mailinator (test.unergie.com)",
        website="mailinator.com",
        generate=lambda o: test_unergie_com.generate_email(),
        get_emails=lambda e, t: test_unergie_com.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="torch-yi-org",
        name="Mailinator (torch.yi.org)",
        website="mailinator.com",
        generate=lambda o: torch_yi_org.generate_email(),
        get_emails=lambda e, t: torch_yi_org.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="t-zibet-net",
        name="Mailinator (t.zibet.net)",
        website="mailinator.com",
        generate=lambda o: t_zibet_net.generate_email(),
        get_emails=lambda e, t: t_zibet_net.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="xue32-buzz",
        name="Mailmomy (xue32.buzz)",
        website="mailmomy.com",
        generate=lambda o: xue32_buzz.generate_email(),
        get_emails=lambda e, t: xue32_buzz.get_emails(t or e),
    )
)
register_channel(
    ChannelSpec(
        channel="apihz",
        name="ApiHz TempMail",
        website="apihz.cn",
        generate=lambda o: apihz.generate_email(),
        get_emails=lambda e, t: apihz.get_emails(_require_token(t, "apihz")),
    )
)
register_channel(
    ChannelSpec(
        channel="sogetthis-com",
        name="Mailinator (sogetthis.com)",
        website="mailinator.com",
        generate=lambda o: sogetthis_com.generate_email(),
        get_emails=lambda e, t: sogetthis_com.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="bobmail-info",
        name="Mailinator (bobmail.info)",
        website="mailinator.com",
        generate=lambda o: bobmail_info.generate_email(),
        get_emails=lambda e, t: bobmail_info.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="suremail-info",
        name="Mailinator (suremail.info)",
        website="mailinator.com",
        generate=lambda o: suremail_info.generate_email(),
        get_emails=lambda e, t: suremail_info.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="binkmail-com",
        name="Mailinator (binkmail.com)",
        website="mailinator.com",
        generate=lambda o: binkmail_com.generate_email(),
        get_emails=lambda e, t: binkmail_com.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="veryrealemail-com",
        name="Mailinator (veryrealemail.com)",
        website="mailinator.com",
        generate=lambda o: veryrealemail_com.generate_email(),
        get_emails=lambda e, t: veryrealemail_com.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="mailmomy",
        name="Mailmomy",
        website="mailmomy.com",
        generate=lambda o: mailmomy.generate_email(),
        get_emails=lambda e, t: mailmomy.get_emails(t or e),
    )
)
register_channel(
    ChannelSpec(
        channel="chammy-info",
        name="Mailinator (chammy.info)",
        website="mailinator.com",
        generate=lambda o: chammy_info.generate_email(),
        get_emails=lambda e, t: chammy_info.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="thisisnotmyrealemail-com",
        name="Mailinator (thisisnotmyrealemail.com)",
        website="mailinator.com",
        generate=lambda o: thisisnotmyrealemail_com.generate_email(),
        get_emails=lambda e, t: thisisnotmyrealemail_com.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="notmailinator-com",
        name="Mailinator (notmailinator.com)",
        website="mailinator.com",
        generate=lambda o: notmailinator_com.generate_email(),
        get_emails=lambda e, t: notmailinator_com.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="spamhereplease-com",
        name="Mailinator (spamhereplease.com)",
        website="mailinator.com",
        generate=lambda o: spamhereplease_com.generate_email(),
        get_emails=lambda e, t: spamhereplease_com.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="sendspamhere-com",
        name="Mailinator (sendspamhere.com)",
        website="mailinator.com",
        generate=lambda o: sendspamhere_com.generate_email(),
        get_emails=lambda e, t: sendspamhere_com.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="sendfree-org",
        name="Mailinator (sendfree.org)",
        website="mailinator.com",
        generate=lambda o: sendfree_org.generate_email(),
        get_emails=lambda e, t: sendfree_org.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="junk-beats-org",
        name="Mailinator (junk.beats.org)",
        website="mailinator.com",
        generate=lambda o: junk_beats_org.generate_email(),
        get_emails=lambda e, t: junk_beats_org.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="junk-ihmehl-com",
        name="Mailinator (junk.ihmehl.com)",
        website="mailinator.com",
        generate=lambda o: junk_ihmehl_com.generate_email(),
        get_emails=lambda e, t: junk_ihmehl_com.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="junk-noplay-org",
        name="Mailinator (junk.noplay.org)",
        website="mailinator.com",
        generate=lambda o: junk_noplay_org.generate_email(),
        get_emails=lambda e, t: junk_noplay_org.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="junk-vanillasystem-com",
        name="Mailinator (junk.vanillasystem.com)",
        website="mailinator.com",
        generate=lambda o: junk_vanillasystem_com.generate_email(),
        get_emails=lambda e, t: junk_vanillasystem_com.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="spam-jasonpearce-com",
        name="Mailinator (spam.jasonpearce.com)",
        website="mailinator.com",
        generate=lambda o: spam_jasonpearce_com.generate_email(),
        get_emails=lambda e, t: spam_jasonpearce_com.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="fish-skytale-net",
        name="Mailinator (fish.skytale.net)",
        website="mailinator.com",
        generate=lambda o: fish_skytale_net.generate_email(),
        get_emails=lambda e, t: fish_skytale_net.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="spam-mccrew-com",
        name="Mailinator (spam.mccrew.com)",
        website="mailinator.com",
        generate=lambda o: spam_mccrew_com.generate_email(),
        get_emails=lambda e, t: spam_mccrew_com.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="dropmail-click",
        name="DropMail.click",
        website="dropmail.click",
        generate=lambda o: dropmail_click.generate_email(),
        get_emails=lambda e, t: dropmail_click.get_emails(t or e),
    )
)
register_channel(
    ChannelSpec(
        channel="spam-coroiu-com",
        name="Mailinator (spam.coroiu.com)",
        website="mailinator.com",
        generate=lambda o: spam_coroiu_com.generate_email(),
        get_emails=lambda e, t: spam_coroiu_com.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="spam-deluser-net",
        name="Mailinator (spam.deluser.net)",
        website="mailinator.com",
        generate=lambda o: spam_deluser_net.generate_email(),
        get_emails=lambda e, t: spam_deluser_net.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="spam-dhsf-net",
        name="Mailinator (spam.dhsf.net)",
        website="mailinator.com",
        generate=lambda o: spam_dhsf_net.generate_email(),
        get_emails=lambda e, t: spam_dhsf_net.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="spam-lucatnt-com",
        name="Mailinator (spam.lucatnt.com)",
        website="mailinator.com",
        generate=lambda o: spam_lucatnt_com.generate_email(),
        get_emails=lambda e, t: spam_lucatnt_com.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="spam-lyceum-life-com-ru",
        name="Mailinator (spam.lyceum-life.com.ru)",
        website="mailinator.com",
        generate=lambda o: spam_lyceum_life_com_ru.generate_email(),
        get_emails=lambda e, t: spam_lyceum_life_com_ru.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="spam-netpirates-net",
        name="Mailinator (spam.netpirates.net)",
        website="mailinator.com",
        generate=lambda o: spam_netpirates_net.generate_email(),
        get_emails=lambda e, t: spam_netpirates_net.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="spam-no-ip-net",
        name="Mailinator (spam.no-ip.net)",
        website="mailinator.com",
        generate=lambda o: spam_no_ip_net.generate_email(),
        get_emails=lambda e, t: spam_no_ip_net.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="spam-ozh-org",
        name="Mailinator (spam.ozh.org)",
        website="mailinator.com",
        generate=lambda o: spam_ozh_org.generate_email(),
        get_emails=lambda e, t: spam_ozh_org.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="spam-pyphus-org",
        name="Mailinator (spam.pyphus.org)",
        website="mailinator.com",
        generate=lambda o: spam_pyphus_org.generate_email(),
        get_emails=lambda e, t: spam_pyphus_org.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="spam-shep-pw",
        name="Mailinator (spam.shep.pw)",
        website="mailinator.com",
        generate=lambda o: spam_shep_pw.generate_email(),
        get_emails=lambda e, t: spam_shep_pw.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="spam-wtf-at",
        name="Mailinator (spam.wtf.at)",
        website="mailinator.com",
        generate=lambda o: spam_wtf_at.generate_email(),
        get_emails=lambda e, t: spam_wtf_at.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="spam-wulczer-org",
        name="Mailinator (spam.wulczer.org)",
        website="mailinator.com",
        generate=lambda o: spam_wulczer_org.generate_email(),
        get_emails=lambda e, t: spam_wulczer_org.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="crap-kakadua-net",
        name="Mailinator (crap.kakadua.net)",
        website="mailinator.com",
        generate=lambda o: crap_kakadua_net.generate_email(),
        get_emails=lambda e, t: crap_kakadua_net.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="spam-janlugt-nl",
        name="Mailinator (spam.janlugt.nl)",
        website="mailinator.com",
        generate=lambda o: spam_janlugt_nl.generate_email(),
        get_emails=lambda e, t: spam_janlugt_nl.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="min-burningfish-net",
        name="Mailinator (min.burningfish.net)",
        website="mailinator.com",
        generate=lambda o: min_burningfish_net.generate_email(),
        get_emails=lambda e, t: min_burningfish_net.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="sink-fblay-com",
        name="Mailinator (sink.fblay.com)",
        website="mailinator.com",
        generate=lambda o: sink_fblay_com.generate_email(),
        get_emails=lambda e, t: sink_fblay_com.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="tempgmailer",
        name="TempGmailer",
        website="tempgmailer.com",
        generate=lambda o: tempgmailer.generate_email(),
        get_emails=lambda e, t: tempgmailer.get_emails(
            _require_token(t, "tempgmailer"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="temp-mail-org",
        name="Temp-Mail.org",
        website="temp-mail.org",
        generate=lambda o: temp_mail_org.generate_email(),
        get_emails=lambda e, t: temp_mail_org.get_emails(
            _require_token(t, "temp-mail-org"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="xkx-me",
        name="XKX.me",
        website="xkx.me",
        generate=lambda o: xkx_me.generate_email(),
        get_emails=lambda e, t: xkx_me.get_emails(_require_token(t, "xkx-me"), e),
    )
)
register_channel(
    ChannelSpec(
        channel="gonebox-email",
        name="GoneBox.email",
        website="gonebox.email",
        generate=lambda o: gonebox_email.generate_email(),
        get_emails=lambda e, t: gonebox_email.get_emails(t or "", e),
    )
)
register_channel(
    ChannelSpec(
        channel="mailcat-ai",
        name="Mailcat.ai",
        website="mailcat.ai",
        generate=lambda o: mailcat_ai.generate_email(),
        get_emails=lambda e, t: mailcat_ai.get_emails(
            _require_token(t, "mailcat-ai"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="tempgo-email",
        name="TempGo Email",
        website="tempgo.email",
        generate=lambda o: tempgo_email.generate_email(),
        get_emails=lambda e, t: tempgo_email.get_emails(
            _require_token(t, "tempgo-email"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="restmail-net",
        name="Restmail.net",
        website="restmail.net",
        generate=lambda o: restmail_net.generate_email(),
        get_emails=lambda e, t: restmail_net.get_emails(e),
    )
)
register_channel(
    ChannelSpec(
        channel="dropmail-me",
        name="Dropmail.me",
        website="dropmail.me",
        generate=lambda o: dropmail_me.generate_email(),
        get_emails=lambda e, t: dropmail_me.get_emails(
            _require_token(t, "dropmail-me"), e
        ),
    )
)
register_channel(
    ChannelSpec(
        channel="ten-minute-mail-net",
        name="10MinuteMail.net",
        website="10minutemail.net",
        generate=lambda o: ten_minute_mail_net.generate_email(),
        get_emails=lambda e, t: ten_minute_mail_net.get_emails(
            _require_token(t, "ten-minute-mail-net"), e
        ),
    )
)
