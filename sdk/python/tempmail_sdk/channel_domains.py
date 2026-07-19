"""渠道域名映射，用于后缀筛选"""

from typing import Dict, List

# 渠道到已知支持域名的静态映射
CHANNEL_DOMAINS: Dict[str, List[str]] = {
    # Gmail 相关
    "emailnator": ["gmail.com", "googlemail.com"],
    "tempgmailer": ["gmail.com"],

    # 多域名渠道
    "catchmail": ["catchmail.io", "mailistry.com", "zeppost.com"],
    "catchmail-mailistry": ["mailistry.com"],
    "catchmail-zeppost": ["zeppost.com"],
    "mailforspam": ["mailforspam.com", "tempmail.io", "disposable.email"],
    "mailforspam-tempmail-io": ["tempmail.io"],
    "mailforspam-disposable": ["disposable.email"],
    "tempmail365": ["fengyou.cc", "shop345.com", "nutemail.com", "qvrf.cn"],
    "tempinbox": ["tempinbox.xyz", "thepiratebay.cloud", "cryptoblad.nl"],
    "10minute-one": ["xghff.com", "oqqaj.com", "psovv.com", "dbwot.com", "ygwpr.com", "imxwe.com"],
    "xghff-com": ["xghff.com"],
    "oqqaj-com": ["oqqaj.com"],
    "psovv-com": ["psovv.com"],
    "dbwot-com": ["dbwot.com"],
    "ygwpr-com": ["ygwpr.com"],
    "imxwe-com": ["imxwe.com"],
    "apihz": ["apimail.email", "apimail.vip"],
    "24mail-chacuo": ["chacuo.net", "027168.com"],

    # mailinator 系列
    "mailinator": ["mailinator.com"],
    "sogetthis-com": ["sogetthis.com"],
    "bobmail-info": ["bobmail.info"],
    "suremail-info": ["suremail.info"],
    "binkmail-com": ["binkmail.com"],
    "veryrealemail-com": ["veryrealemail.com"],
    "chammy-info": ["chammy.info"],
    "thisisnotmyrealemail-com": ["thisisnotmyrealemail.com"],
    "notmailinator-com": ["notmailinator.com"],
    "spamhereplease-com": ["spamhereplease.com"],
    "sendspamhere-com": ["sendspamhere.com"],
    "sendfree-org": ["sendfree.org"],

    # 其他单域名
    "mailnesia": ["mailnesia.com"],
    "harakirimail": ["harakirimail.com"],
    "byom": ["byom.de"],
    "eyepaste": ["eyepaste.com"],
    "mailcatch": ["mailcatch.com"],
    "maildrop-cc": ["maildrop.cc"],
    "smail-pw": ["smail.pw"],
    "inboxkitten": ["inboxkitten.com"],
    "mffac": ["mffac.com"],
    "nimail": ["nimail.cn"],
    "mailgolem": ["mailgolem.com"],
    "mohmal": ["emailinbo.live"],
    "minuteinbox": ["minafter.com"],

    # tempmail-plus 系列
    "tempmail-plus": ["mailto.plus"],
    "fexpost-com": ["fexpost.com"],
    "fexbox-org": ["fexbox.org"],
    "mailbox-in-ua": ["mailbox.in.ua"],
    "rover-info": ["rover.info"],
    "chitthi-in": ["chitthi.in"],
    "fextemp-com": ["fextemp.com"],
    "any-pink": ["any.pink"],
    "merepost-com": ["merepost.com"],

    # getnada 系列
    "getnada": ["getnada.com"],
    "getnada-com": ["getnada.com"],
    "getnada-email": ["getnada.email"],
    "getnada-net": ["getnada.net"],

    # disposablemail
    "disposablemail-app": ["disposablemail.dev", "mailmehere.cc"],



    # xkx.me
    "xkx-me": ["xkx.me"],
}

# 域名通过 API 动态获取的渠道（筛选时默认保留）
DYNAMIC_DOMAIN_CHANNELS = {
    "mail-tm", "duckmail", "mail-td", "chatgpt-org-uk", "temporam",
    "getnada", "neighbours", "neighbours-sh", "inboxes", "mailmomy",
    "freecustom", "mail-sunls", "anonymmail", "fake-legal", "maildrop",
    "mail-cx", "moakt", "tempmail-lol", "smailpro", "tempmail",
    "1sec-mail", "guerrillamail", "dropmail", "fmail", "ockito",
    "temp-mail-org",
}


def filter_channels_by_domain(channels: List[str], target_domains: List[str]) -> List[str]:
    """根据目标域名过滤渠道列表"""
    if not target_domains:
        return channels

    filtered = []
    for ch in channels:
        # 动态域名渠道默认保留
        if ch in DYNAMIC_DOMAIN_CHANNELS:
            filtered.append(ch)
            continue
        # 检查该渠道是否支持目标域名
        domains = CHANNEL_DOMAINS.get(ch)
        if not domains:
            continue
        if _matches_domain(domains, target_domains):
            filtered.append(ch)

    # 过滤后为空则回退到原列表
    return filtered if filtered else channels


def _matches_domain(channel_domains: List[str], target_domains: List[str]) -> bool:
    """检查渠道支持的域名是否匹配任一目标域名"""
    for cd in channel_domains:
        for td in target_domains:
            if cd == td or cd.endswith("." + td):
                return True
    return False
