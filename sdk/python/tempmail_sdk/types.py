"""
SDK 类型定义
"""

from dataclasses import dataclass, field
from typing import Optional, List, Literal, Any

# 支持的临时邮箱渠道标识
Channel = Literal[
    "tempmail",
    "tempmail-cn",
    "ta-easy",
    "10minute-one",
    "xghff-com",
    "oqqaj-com",
    "psovv-com",
    "dbwot-com",
    "ygwpr-com",
    "imxwe-com",
    "linshiyou",
    "mffac",
    "tempmail-lol",
    "chatgpt-org-uk",
    "temp-mail-io",
    "mail-cx",
    "ddker-com",
    "catchmail",
    "catchmail-mailistry",
    "catchmail-zeppost",
    "mailforspam",
    "mailforspam-tempmail-io",
    "mailforspam-disposable",
    "tempmailc",
    "mailnesia",
    "throwawaymail",
    "shitty-email",
    "tempmailpro",
    "devmail-uk",
    "cleantempmail",
    "inboxkitten",
    "getnada",
    "1vpn-net",
    "abematv-com",
    "abematv-net",
    "abematv-org",
    "aceh-cc",
    "bangkabelitung-net",
    "cctruyen-com",
    "getnada-com",
    "getnada-email",
    "getnada-net",
    "jawatengah-net",
    "jawatimur-net",
    "kalimantanbarat-net",
    "kalimantanselatan-net",
    "kalimantantengah-net",
    "kalimantantimur-net",
    "kalimantanutara-net",
    "kepulauanriau-net",
    "luxury345-com",
    "malukuutara-net",
    "nusatenggarabarat-net",
    "nusatenggaratimur-net",
    "papuabarat-net",
    "papuabaratdaya-net",
    "papuaselatan-net",
    "pehol-com",
    "ptruyen-com",
    "pulaubali-net",
    "riau-net",
    "seokey-org",
    "sulawesibarat-net",
    "sulawesiselatan-net",
    "sulawesitengah-net",
    "sulawesitenggara-net",
    "sumaterabarat-net",
    "sumateraselatan-net",
    "sumaterautara-net",
    "villatogel-com",
    "mail123",
    "mail10s",
    "webmailtemp",
    "tempfastmail",
    "1sec-mail",
    "fakemail",
    "openinbox",
    "inboxes",
    "uncorreotemporal",
    "awamail",
    "mail-tm",
    "web-library-net",
    "dropmail",
    "guerrillamail",
    "guerrillamail-com",
    "maildrop",
    "smail-pw",
    "vip-215",
    "fake-legal",
    "imgui-de",
    "pulsewebmenu-de",
    "moakt",
    "drmail-in",
    "teml-net",
    "tmpeml-com",
    "tmpbox-net",
    "moakt-cc",
    "disbox-net",
    "tmpmail-org",
    "tmpmail-net",
    "tmails-net",
    "disbox-org",
    "moakt-co",
    "moakt-ws",
    "tmail-ws",
    "bareed-ws",
    "email10min",
    "mjj-cm",
    "linshi-co",
    "harakirimail",
    "jqkjqk-xyz",
    "lyhlevi-com",
    "tempmail-plus",
    "fexpost-com",
    "fexbox-org",
    "mailbox-in-ua",
    "rover-info",
    "chitthi-in",
    "fextemp-com",
    "any-pink",
    "merepost-com",
    "tempmail-lol-v2",
    "tempgbox",
    "emailnator",
    "temporam",
    "sharklasers",
    "neighbours",
    "sharklasers-com",
    "grr-la",
    "grr-la-com",
    "guerrillamail-info",
    "spam4me",
    "guerrillamail-net",
    "guerrillamail-org",
    "guerrillamailblock",
    "guerrillamail-com-www",
    "m2u",
    "tempy-email",
    "fmail",
    "ockito",
    "anonbox",
    "duckmail",
    "mailinator",
    "sogetthis-com",
    "bobmail-info",
    "suremail-info",
    "binkmail-com",
    "veryrealemail-com",
    "chammy-info",
    "thisisnotmyrealemail-com",
    "notmailinator-com",
    "tempmail365",
    "tempinbox",
    "byom",
    "anonymmail",
    "eyepaste",
    "mail-sunls",
    "expressinboxhub",
    "lroid",
    "haribu",
    "rootsh",
    "fake-email-site",
    "mohmal",
    "mailgolem",
    "best-temp-mail",
    "disposablemail-app",
    "mailtemp-cc",
    "minuteinbox",
    "mailcatch",
    "tempemail-co",
    "tempemails-net",
    "altmails",
    "tempemail-info",
    "smailpro",
    "tempmailten",
    "maildrop-cc",
    "10minutemail-net",
    "linshiyouxiang-net",
    "tempmail-fyi",
    "disposablemail-com",
    "tempp-mails",
    "emailtemp-org",
    "mytempmail-cc",
    "temp-mail-now",
    "mail-td",
    "mailhole-de",
    "tmail-link",
    "24mail-chacuo",
    "nimail",
    "freecustom",
    "apihz",
    "mailmomy",
    "spamhereplease-com",
    "sendspamhere-com",
    "sendfree-org",
    "junk-beats-org",
    "junk-ihmehl-com",
    "junk-noplay-org",
    "junk-vanillasystem-com",
    "spam-jasonpearce-com",
    "spam-coroiu-com",
    "spam-deluser-net",
    "spam-dhsf-net",
    "spam-lucatnt-com",
    "spam-lyceum-life-com-ru",
    "spam-netpirates-net",
    "spam-no-ip-net",
    "spam-ozh-org",
    "spam-pyphus-org",
    "spam-shep-pw",
    "spam-wtf-at",
    "spam-wulczer-org",
    "crap-kakadua-net",
    "spam-janlugt-nl",
    "min-burningfish-net",
    "sink-fblay-com",
    "etgdev-de",
    "mtmdev-com",
    "test-unergie-com",
    "block-bdea-cc",
    "torch-yi-org",
    "carlton183-changeip-net",
    "mail-fsmash-org",
    "ebs-com-ar",
    "jama-trenet-eu",
    "blackhole-djurby-se",
    "m8r-davidfuhr-de",
    "mi-meon-be",
    "m-nik-me",
    "mail-bentrask-com",
    "t-zibet-net",
    "m8r-mcasal-com",
    "ramjane-mooo-com",
    "rauxa-seny-cat",
    "sp-woot-at",
    "fwd2m-eszett-es",
    "m-887-at",
    "nospam-thurstons-us",
    "null-k3vin-net",
    "really-istrash-com",
    "spam-hortuk-ovh",
    "fish-skytale-net",
    "spam-mccrew-com",
    "dropmail-click",
    "16888888-cyou",
    "17666688-shop",
    "282mail-com",
    "bsdu32-buzz",
    "b-smelly-cc",
    "dea-soon-it",
    "disposable-al-sudani-com",
    "disposable-nogonad-nl",
    "doxu243-buzz",
    "easyme-pro",
    "evergreenco-shop",
    "j-fairuse-org",
    "layueming-pics",
    "mailinatorzz-mooo-com",
    "mingyuekeji-online",
    "mingyueming-click",
    "mingyueming-shop",
    "mingyukeji-lol",
    "mn-curppa-com",
    "notfond-404-mn",
    "nuxh62-space",
    "proid-cloud-ip-cc",
    "sbook-pics",
    "xue32-buzz",
]


class EmailInfo:
    """
    创建临时邮箱后返回的邮箱信息
    Token 等认证信息由 SDK 内部维护，不对外暴露
    """

    def __init__(
        self,
        channel: str,
        email: str,
        *,
        _token: Optional[str] = None,
        expires_at: Optional[int] = None,
        created_at: Optional[Any] = None,
    ):
        self.channel = channel  # 创建该邮箱所使用的渠道
        self.email = email  # 临时邮箱地址
        self._token = _token  # 认证令牌，SDK 内部维护，不对外暴露
        self.expires_at = expires_at  # 邮箱过期时间（毫秒时间戳）
        self.created_at = created_at  # 邮箱创建时间

    def __repr__(self):
        return f"EmailInfo(channel={self.channel!r}, email={self.email!r}, expires_at={self.expires_at!r}, created_at={self.created_at!r})"


@dataclass
class EmailAttachment:
    """邮件附件信息"""

    filename: str = ""  # 附件文件名
    size: Optional[int] = None  # 附件大小（字节）
    content_type: Optional[str] = None  # 附件 MIME 类型
    url: Optional[str] = None  # 附件下载地址


@dataclass
class Email:
    """标准化的邮件对象"""

    id: str = ""  # 邮件唯一标识
    from_addr: str = ""  # 发件人地址
    to: str = ""  # 收件人地址
    subject: str = ""  # 邮件主题
    text: str = ""  # 纯文本内容
    html: str = ""  # HTML 内容
    date: str = ""  # 接收日期（ISO 8601 格式）
    is_read: bool = False  # 是否已读
    attachments: List[EmailAttachment] = field(default_factory=list)  # 附件列表


@dataclass
class GetEmailsResult:
    """获取邮件列表的结果"""

    channel: str = ""  # 渠道标识
    email: str = ""  # 邮箱地址
    emails: List[Email] = field(default_factory=list)  # 邮件列表
    success: bool = True  # 本次请求是否成功


@dataclass
class RetryConfig:
    """重试配置"""

    max_retries: int = 2  # 最大重试次数（不含首次请求），默认 2
    initial_delay: float = 1.0  # 初始重试延迟（秒），默认 1.0
    max_delay: float = 5.0  # 最大重试延迟（秒），默认 5.0
    timeout: float = 15.0  # 请求超时（秒），默认 15.0


@dataclass
class GenerateEmailOptions:
    """创建临时邮箱的选项"""

    channel: Optional[str] = None  # 指定渠道，不指定则随机选择
    duration: int = 30  # tempmail 渠道的有效期（分钟）
    domain: Optional[str] = (
        None  # tempmail-cn 接入域名，或 tempmail-lol / maildrop / fake-legal / mail-cx 指定域名
    )
    retry: Optional[RetryConfig] = None  # 重试配置


@dataclass
class GetEmailsOptions:
    """
    获取邮件的选项
    Channel/Email/Token 等由 SDK 从 EmailInfo 中自动获取，用户无需手动传递
    """

    retry: Optional[RetryConfig] = None  # 重试配置


@dataclass
class ChannelInfo:
    """渠道信息"""

    channel: str = ""  # 渠道标识
    name: str = ""  # 渠道显示名称
    website: str = ""  # 对应的临时邮箱服务网站
