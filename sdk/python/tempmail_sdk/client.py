"""
SDK 主入口
聚合所有渠道的逻辑，提供统一的 API
"""

import random
from typing import Optional, List
from .types import (
    EmailInfo, Email, GetEmailsResult,
    GenerateEmailOptions, GetEmailsOptions,
    ChannelInfo,
)
from .retry import with_retry, with_retry_with_attempts
from .telemetry import report_telemetry
from .logger import get_logger
from .providers import (
    tempmail, tempmail_cn, ta_easy, linshiyou, mffac, tempmail_lol, chatgpt_org_uk,
    awamail, mail_tm,
    dropmail, guerrillamail, maildrop, smail_pw,
    vip_215, fake_legal, moakt, tenminute_one,
    email10min, mjj_cm, linshi_co,
    harakirimail, tempmail_plus,
    mail_gw, tempmail_lol_v2, guerrillamail_mirrors, temp_mail_io, mail_cx,
    catchmail, mailforspam, tempmailo, tempmailc, mailnesia, throwawaymail, inboxkitten, getnada, mail123, one_sec_mail, fakemail, openinbox, inboxes, uncorreotemporal,
)

# 所有支持的渠道列表
ALL_CHANNELS = [
    "tempmail", "tempmail-cn", "ta-easy", "10minute-one", "linshiyou", "mffac", "tempmail-lol", "chatgpt-org-uk",
    "temp-mail-io", "mail-cx", "catchmail", "catchmail-mailistry", "catchmail-zeppost", "mailforspam", "mailforspam-tempmail-io", "mailforspam-disposable", "tempmailo", "tempmailc", "mailnesia", "throwawaymail", "inboxkitten", "getnada", "mail123", "1sec-mail", "fakemail", "openinbox", "inboxes", "uncorreotemporal",
    "awamail", "mail-tm", "dropmail", "guerrillamail", "guerrillamail-com", "maildrop", "smail-pw",
    "vip-215", "fake-legal", "moakt", "email10min", "mjj-cm", "linshi-co",
    "harakirimail", "tempmail-plus",
    "mail-gw", "tempmail-lol-v2", "sharklasers", "sharklasers-com", "grr-la", "grr-la-com", "guerrillamail-info", "spam4me",
    "guerrillamail-net", "guerrillamail-org", "guerrillamailblock", "guerrillamail-com-www",
]

# 渠道信息映射表
CHANNEL_INFO_MAP = {
    "tempmail": ChannelInfo(channel="tempmail", name="TempMail", website="tempmail.ing"),
    "tempmail-cn": ChannelInfo(channel="tempmail-cn", name="TempMail CN", website="tempmail.cn"),
    "ta-easy": ChannelInfo(channel="ta-easy", name="TA Easy", website="ta-easy.com"),
    "10minute-one": ChannelInfo(channel="10minute-one", name="10 Minute Email", website="10minutemail.one"),
    "linshiyou": ChannelInfo(channel="linshiyou", name="临时邮", website="linshiyou.com"),
    "mffac": ChannelInfo(channel="mffac", name="MFFAC", website="mffac.com"),
    "tempmail-lol": ChannelInfo(channel="tempmail-lol", name="TempMail LOL", website="tempmail.lol"),
    "chatgpt-org-uk": ChannelInfo(channel="chatgpt-org-uk", name="ChatGPT Mail", website="mail.chatgpt.org.uk"),
    "temp-mail-io": ChannelInfo(channel="temp-mail-io", name="Temp-Mail.io", website="temp-mail.io"),
    "mail-cx": ChannelInfo(channel="mail-cx", name="Mail.cx", website="mail.cx"),
    "catchmail": ChannelInfo(channel="catchmail", name="Catchmail", website="catchmail.io"),
    "catchmail-mailistry": ChannelInfo(channel="catchmail-mailistry", name="Catchmail Mailistry", website="mailistry.com"),
    "catchmail-zeppost": ChannelInfo(channel="catchmail-zeppost", name="Catchmail Zeppost", website="zeppost.com"),
    "mailforspam": ChannelInfo(channel="mailforspam", name="MailForSpam", website="mailforspam.com"),
    "mailforspam-tempmail-io": ChannelInfo(channel="mailforspam-tempmail-io", name="MailForSpam TempMail.io", website="tempmail.io"),
    "mailforspam-disposable": ChannelInfo(channel="mailforspam-disposable", name="MailForSpam Disposable", website="disposable.email"),
    "tempmailo": ChannelInfo(channel="tempmailo", name="Tempmailo", website="tempmailo.com"),
    "tempmailc": ChannelInfo(channel="tempmailc", name="TempMailC", website="tempmailc.com"),
    "mailnesia": ChannelInfo(channel="mailnesia", name="Mailnesia", website="mailnesia.com"),
    "throwawaymail": ChannelInfo(channel="throwawaymail", name="ThrowawayMail", website="throwawaymail.app"),
    "inboxkitten": ChannelInfo(channel="inboxkitten", name="InboxKitten", website="inboxkitten.com"),
    "getnada": ChannelInfo(channel="getnada", name="GetNada", website="getnada.net"),
    "mail123": ChannelInfo(channel="mail123", name="Mail123", website="mail123.fr"),
    "1sec-mail": ChannelInfo(channel="1sec-mail", name="1SecMail", website="1sec-mail.com"),
    "fakemail": ChannelInfo(channel="fakemail", name="FakeMail", website="fakemail.net"),
    "openinbox": ChannelInfo(channel="openinbox", name="OpenInbox", website="openinbox.io"),
    "inboxes": ChannelInfo(channel="inboxes", name="Inboxes", website="inboxes.com"),
    "uncorreotemporal": ChannelInfo(channel="uncorreotemporal", name="UnCorreoTemporal", website="uncorreotemporal.com"),
    "awamail": ChannelInfo(channel="awamail", name="AwaMail", website="awamail.com"),
    "mail-tm": ChannelInfo(channel="mail-tm", name="Mail.tm", website="mail.tm"),
    "dropmail": ChannelInfo(channel="dropmail", name="DropMail", website="dropmail.me"),
    "guerrillamail": ChannelInfo(channel="guerrillamail", name="Guerrilla Mail", website="guerrillamail.com"),
    "guerrillamail-com": ChannelInfo(channel="guerrillamail-com", name="GuerrillaMail Root", website="guerrillamail.com"),
    "maildrop": ChannelInfo(channel="maildrop", name="Maildrop", website="maildrop.cx"),
    "smail-pw": ChannelInfo(channel="smail-pw", name="Smail.pw", website="smail.pw"),
    "vip-215": ChannelInfo(channel="vip-215", name="VIP 215", website="vip.215.im"),
    "fake-legal": ChannelInfo(channel="fake-legal", name="Fake Legal", website="fake.legal"),
    "moakt": ChannelInfo(channel="moakt", name="Moakt", website="moakt.com"),
    "email10min": ChannelInfo(channel="email10min", name="Email10Min", website="email10min.com"),
    "mjj-cm": ChannelInfo(channel="mjj-cm", name="MJJ Mail", website="mjj.cm"),
    "linshi-co": ChannelInfo(channel="linshi-co", name="临时Co", website="linshi.co"),
    "harakirimail": ChannelInfo(channel="harakirimail", name="HarakiriMail", website="harakirimail.com"),
    "tempmail-plus": ChannelInfo(channel="tempmail-plus", name="TempMail Plus", website="tempmail.plus"),
    "mail-gw": ChannelInfo(channel="mail-gw", name="Mail.gw", website="mail.gw"),
    "tempmail-lol-v2": ChannelInfo(channel="tempmail-lol-v2", name="TempMail LOL V2", website="tempmail.lol"),
    "sharklasers": ChannelInfo(channel="sharklasers", name="SharkLasers", website="sharklasers.com"),
    "sharklasers-com": ChannelInfo(channel="sharklasers-com", name="SharkLasers Root", website="sharklasers.com"),
    "grr-la": ChannelInfo(channel="grr-la", name="Grr.la", website="grr.la"),
    "grr-la-com": ChannelInfo(channel="grr-la-com", name="Grr.la Root", website="grr.la"),
    "guerrillamail-info": ChannelInfo(channel="guerrillamail-info", name="GuerrillaMail Info", website="guerrillamail.info"),
    "spam4me": ChannelInfo(channel="spam4me", name="Spam4.me", website="spam4.me"),
    "guerrillamail-net": ChannelInfo(channel="guerrillamail-net", name="GuerrillaMail Net", website="guerrillamail.net"),
    "guerrillamail-org": ChannelInfo(channel="guerrillamail-org", name="GuerrillaMail Org", website="guerrillamail.org"),
    "guerrillamailblock": ChannelInfo(channel="guerrillamailblock", name="GuerrillaMailBlock", website="guerrillamailblock.com"),
    "guerrillamail-com-www": ChannelInfo(channel="guerrillamail-com-www", name="GuerrillaMail WWW", website="guerrillamail.com"),
}


def list_channels() -> List[ChannelInfo]:
    """获取所有支持的渠道列表"""
    return [CHANNEL_INFO_MAP[ch] for ch in ALL_CHANNELS]


def get_channel_info(channel: str) -> Optional[ChannelInfo]:
    """获取指定渠道的详细信息"""
    return CHANNEL_INFO_MAP.get(channel)


def _with_channel(info: EmailInfo, channel: str) -> EmailInfo:
    info.channel = channel
    return info


def generate_email(options: Optional[GenerateEmailOptions] = None) -> Optional[EmailInfo]:
    """
    创建临时邮箱

    错误处理策略:
    - 指定渠道失败时，自动尝试其他可用渠道（打乱顺序逐个尝试）
    - 未指定渠道时，打乱全部渠道逐个尝试，直到成功
    - 所有渠道均不可用时返回 None（不抛出异常）

    示例:
        info = generate_email(GenerateEmailOptions(channel="guerrillamail"))
        if info:
            print(info.email)
    """
    if options is None:
        options = GenerateEmailOptions()

    logger = get_logger()

    # 构建尝试顺序：指定渠道优先尝试，失败后随机尝试其他渠道；未指定则打乱全部逐个尝试
    try_order = _build_channel_order(options.channel)

    channels_tried = 0
    last_err = ""
    for ch in try_order:
        channels_tried += 1
        logger.info(f"创建临时邮箱, 渠道: {ch}")
        r = with_retry_with_attempts(
            lambda c=ch: _generate_email_once(c, options),
            options.retry,
        )
        if r.ok:
            result = r.value
            assert result is not None
            logger.info(f"邮箱创建成功: {result.email} (渠道: {ch})")
            report_telemetry("generate_email", ch, True, r.attempts, channels_tried, "")
            return result
        err = r.error
        last_err = str(err) if err else "unknown"
        logger.warning(f"渠道 {ch} 不可用: {last_err}，尝试下一个渠道")

    logger.error("所有渠道均不可用，创建邮箱失败")
    report_telemetry("generate_email", "", False, 0, channels_tried, last_err)
    return None


def _build_channel_order(preferred: Optional[str] = None) -> list:
    """
    构建渠道尝试顺序
    指定渠道时优先尝试该渠道，其余渠道打乱追加
    未指定时打乱全部渠道
    """
    shuffled = ALL_CHANNELS[:]
    random.shuffle(shuffled)
    if not preferred:
        return shuffled
    rest = [ch for ch in shuffled if ch != preferred]
    return [preferred] + rest


def _generate_email_once(channel: str, options: GenerateEmailOptions) -> EmailInfo:
    """单次创建邮箱（不含重试逻辑）"""
    if channel == "tempmail":
        return tempmail.generate_email(options.duration)
    elif channel == "tempmail-cn":
        return tempmail_cn.generate_email(options.domain)
    elif channel == "ta-easy":
        return ta_easy.generate_email()
    elif channel == "10minute-one":
        return tenminute_one.generate_email(options.domain)
    elif channel == "linshiyou":
        return linshiyou.generate_email()
    elif channel == "mffac":
        return mffac.generate_email()
    elif channel == "tempmail-lol":
        return tempmail_lol.generate_email(options.domain)
    elif channel == "chatgpt-org-uk":
        return chatgpt_org_uk.generate_email()
    elif channel == "temp-mail-io":
        return temp_mail_io.generate_email()
    elif channel == "mail-cx":
        return mail_cx.generate_email(options.domain)
    elif channel == "catchmail":
        return catchmail.generate_email(options.domain)
    elif channel == "catchmail-mailistry":
        return _with_channel(catchmail.generate_email("mailistry.com"), "catchmail-mailistry")
    elif channel == "catchmail-zeppost":
        return _with_channel(catchmail.generate_email("zeppost.com"), "catchmail-zeppost")
    elif channel == "mailforspam":
        return mailforspam.generate_email(options.domain)
    elif channel == "mailforspam-tempmail-io":
        return _with_channel(mailforspam.generate_email("tempmail.io"), "mailforspam-tempmail-io")
    elif channel == "mailforspam-disposable":
        return _with_channel(mailforspam.generate_email("disposable.email"), "mailforspam-disposable")
    elif channel == "tempmailo":
        return tempmailo.generate_email()
    elif channel == "tempmailc":
        return tempmailc.generate_email()
    elif channel == "mailnesia":
        return mailnesia.generate_email()
    elif channel == "throwawaymail":
        return throwawaymail.generate_email()
    elif channel == "inboxkitten":
        return inboxkitten.generate_email()
    elif channel == "getnada":
        return getnada.generate_email()
    elif channel == "mail123":
        return mail123.generate_email()
    elif channel == "1sec-mail":
        return one_sec_mail.generate_email()
    elif channel == "fakemail":
        return fakemail.generate_email()
    elif channel == "openinbox":
        return openinbox.generate_email()
    elif channel == "inboxes":
        return inboxes.generate_email(options.domain)
    elif channel == "uncorreotemporal":
        return uncorreotemporal.generate_email()
    elif channel == "awamail":
        return awamail.generate_email()
    elif channel == "mail-tm":
        return mail_tm.generate_email()
    elif channel == "dropmail":
        return dropmail.generate_email()
    elif channel == "guerrillamail":
        return guerrillamail.generate_email()
    elif channel == "guerrillamail-com":
        return guerrillamail_mirrors.guerrillamail_com_generate()
    elif channel == "maildrop":
        return maildrop.generate_email(options.domain)
    elif channel == "smail-pw":
        return smail_pw.generate_email()
    elif channel == "vip-215":
        return vip_215.generate_email()
    elif channel == "fake-legal":
        return fake_legal.generate_email(options.domain)
    elif channel == "moakt":
        return moakt.generate_email(options.domain)
    elif channel == "email10min":
        return email10min.generate_email()
    elif channel == "mjj-cm":
        return mjj_cm.generate_email()
    elif channel == "linshi-co":
        return linshi_co.generate_email()
    elif channel == "harakirimail":
        return harakirimail.generate_email()
    elif channel == "tempmail-plus":
        return tempmail_plus.generate_email()
    elif channel == "mail-gw":
        return mail_gw.generate_email()
    elif channel == "tempmail-lol-v2":
        return tempmail_lol_v2.generate_email()
    elif channel == "sharklasers":
        return guerrillamail_mirrors.sharklasers_generate()
    elif channel == "sharklasers-com":
        return guerrillamail_mirrors.sharklasers_com_generate()
    elif channel == "grr-la":
        return guerrillamail_mirrors.grrla_generate()
    elif channel == "grr-la-com":
        return guerrillamail_mirrors.grrla_com_generate()
    elif channel == "guerrillamail-info":
        return guerrillamail_mirrors.guerrillamail_info_generate()
    elif channel == "spam4me":
        return guerrillamail_mirrors.spam4me_generate()
    elif channel == "guerrillamail-net":
        return guerrillamail_mirrors.guerrillamail_net_generate()
    elif channel == "guerrillamail-org":
        return guerrillamail_mirrors.guerrillamail_org_generate()
    elif channel == "guerrillamailblock":
        return guerrillamail_mirrors.guerrillamailblock_generate()
    elif channel == "guerrillamail-com-www":
        return guerrillamail_mirrors.guerrillamail_com_www_generate()
    else:
        raise ValueError(f"Unknown channel: {channel}")


def get_emails(info: EmailInfo, options: Optional[GetEmailsOptions] = None) -> GetEmailsResult:
    """
    获取邮件列表
    Channel/Email/Token 等由 SDK 从 EmailInfo 中自动获取，用户无需手动传递

    错误处理策略:
    - 网络错误、超时、429、HTTP 5xx → 自动重试（默认 2 次）
    - 重试耗尽后返回 GetEmailsResult(success=False, emails=[])
    - 参数校验错误直接抛出异常

    示例:
        info = generate_email(GenerateEmailOptions(channel="mail-tm"))
        result = get_emails(info)
        if result.success:
            print(f"收到 {len(result.emails)} 封邮件")
    """
    if info is None:
        report_telemetry("get_emails", "", False, 0, 0, "EmailInfo is required, call generate_email() first")
        raise ValueError("EmailInfo is required, call generate_email() first")

    channel = info.channel
    email = info.email
    token = info._token
    retry = options.retry if options else None
    logger = get_logger()

    if not channel:
        report_telemetry("get_emails", "", False, 0, 0, "channel is required")
        raise ValueError("channel is required")
    if not email and channel != "tempmail-lol":
        report_telemetry("get_emails", channel, False, 0, 0, "email is required")
        raise ValueError("email is required")

    logger.debug(f"获取邮件, 渠道: {channel}, 邮箱: {email}")

    r = with_retry_with_attempts(
        lambda: _get_emails_once(channel, email, token),
        retry,
    )
    if r.ok:
        emails = r.value
        assert emails is not None
        if emails:
            logger.info(f"获取到 {len(emails)} 封邮件, 渠道: {channel}")
        else:
            logger.debug(f"暂无邮件, 渠道: {channel}")
        report_telemetry("get_emails", channel, True, r.attempts, 0, "")
        return GetEmailsResult(channel=channel, email=email, emails=emails, success=True)

    err = r.error
    err_s = str(err) if err else "unknown"
    logger.error(f"获取邮件失败, 渠道: {channel}, 错误: {err_s}")
    report_telemetry("get_emails", channel, False, r.attempts, 0, err_s)
    return GetEmailsResult(channel=channel, email=email, emails=[], success=False)


def _get_emails_once(channel: str, email: str, token: Optional[str]) -> List[Email]:
    """单次获取邮件（不含重试逻辑）"""
    if channel == "tempmail":
        return tempmail.get_emails(email)
    elif channel == "tempmail-cn":
        return tempmail_cn.get_emails(email)
    elif channel == "ta-easy":
        if not token:
            raise ValueError("token is required for ta-easy channel")
        return ta_easy.get_emails(email, token)
    elif channel == "linshiyou":
        if not token:
            raise ValueError("token is required for linshiyou channel")
        return linshiyou.get_emails(token, email)
    elif channel == "mffac":
        return mffac.get_emails(email, token or "")
    elif channel == "tempmail-lol":
        if not token:
            raise ValueError("token is required for tempmail-lol channel")
        return tempmail_lol.get_emails(token, email)
    elif channel == "chatgpt-org-uk":
        if not token:
            raise ValueError("token is required for chatgpt-org-uk channel")
        return chatgpt_org_uk.get_emails(token, email)
    elif channel == "temp-mail-io":
        return temp_mail_io.get_emails(email)
    elif channel == "mail-cx":
        if not token:
            raise ValueError("token is required for mail-cx channel")
        return mail_cx.get_emails(token, email)
    elif channel == "catchmail":
        return catchmail.get_emails(email)
    elif channel == "catchmail-mailistry":
        return catchmail.get_emails(email)
    elif channel == "catchmail-zeppost":
        return catchmail.get_emails(email)
    elif channel == "mailforspam":
        return mailforspam.get_emails(email)
    elif channel == "mailforspam-tempmail-io":
        return mailforspam.get_emails(email)
    elif channel == "mailforspam-disposable":
        return mailforspam.get_emails(email)
    elif channel == "tempmailo":
        if not token:
            raise ValueError("token is required for tempmailo channel")
        return tempmailo.get_emails(token, email)
    elif channel == "tempmailc":
        return tempmailc.get_emails(email)
    elif channel == "mailnesia":
        return mailnesia.get_emails(email)
    elif channel == "throwawaymail":
        if not token:
            raise ValueError("token is required for throwawaymail channel")
        return throwawaymail.get_emails(token, email)
    elif channel == "inboxkitten":
        return inboxkitten.get_emails(email)
    elif channel == "getnada":
        if not token:
            raise ValueError("token is required for getnada channel")
        return getnada.get_emails(token, email)
    elif channel == "mail123":
        return mail123.get_emails(email)
    elif channel == "1sec-mail":
        if not token:
            raise ValueError("token is required for 1sec-mail channel")
        return one_sec_mail.get_emails(token, email)
    elif channel == "fakemail":
        if not token:
            raise ValueError("token is required for fakemail channel")
        return fakemail.get_emails(token, email)
    elif channel == "openinbox":
        if not token:
            raise ValueError("token is required for openinbox channel")
        return openinbox.get_emails(token, email)
    elif channel == "inboxes":
        return inboxes.get_emails(email)
    elif channel == "uncorreotemporal":
        if not token:
            raise ValueError("token is required for uncorreotemporal channel")
        return uncorreotemporal.get_emails(token, email)
    elif channel == "awamail":
        if not token:
            raise ValueError("token is required for awamail channel")
        return awamail.get_emails(token, email)
    elif channel == "mail-tm":
        if not token:
            raise ValueError("token is required for mail-tm channel")
        return mail_tm.get_emails(token, email)
    elif channel == "dropmail":
        if not token:
            raise ValueError("token is required for dropmail channel")
        return dropmail.get_emails(token, email)
    elif channel == "guerrillamail":
        if not token:
            raise ValueError("token is required for guerrillamail channel")
        return guerrillamail.get_emails(token, email)
    elif channel == "guerrillamail-com":
        if not token:
            raise ValueError("token is required for guerrillamail-com channel")
        return guerrillamail_mirrors.guerrillamail_com_get_emails(token, email)
    elif channel == "maildrop":
        if not token:
            raise ValueError("token is required for maildrop channel")
        return maildrop.get_emails(token, email)
    elif channel == "smail-pw":
        if not token:
            raise ValueError("token is required for smail-pw channel")
        return smail_pw.get_emails(token, email)
    elif channel == "vip-215":
        if not token:
            raise ValueError("token is required for vip-215 channel")
        return vip_215.get_emails(token, email)
    elif channel == "fake-legal":
        return fake_legal.get_emails(email)
    elif channel == "moakt":
        if not token:
            raise ValueError("token is required for moakt channel")
        return moakt.get_emails(email, token)
    elif channel == "10minute-one":
        if not token:
            raise ValueError("token is required for 10minute-one channel")
        return tenminute_one.get_emails(email, token)
    elif channel == "email10min":
        if not token:
            raise ValueError("token is required for email10min channel")
        return email10min.get_emails(email, token)
    elif channel == "mjj-cm":
        return mjj_cm.get_emails(email)
    elif channel == "linshi-co":
        return linshi_co.get_emails(email)
    elif channel == "harakirimail":
        return harakirimail.get_emails(email)
    elif channel == "tempmail-plus":
        return tempmail_plus.get_emails(email)
    elif channel == "mail-gw":
        if not token:
            raise ValueError("token is required for mail-gw channel")
        return mail_gw.get_emails(token, email)
    elif channel == "tempmail-lol-v2":
        if not token:
            raise ValueError("token is required for tempmail-lol-v2 channel")
        return tempmail_lol_v2.get_emails(token, email)
    elif channel == "sharklasers":
        if not token:
            raise ValueError("token is required for sharklasers channel")
        return guerrillamail_mirrors.sharklasers_get_emails(token, email)
    elif channel == "sharklasers-com":
        if not token:
            raise ValueError("token is required for sharklasers-com channel")
        return guerrillamail_mirrors.sharklasers_com_get_emails(token, email)
    elif channel == "grr-la":
        if not token:
            raise ValueError("token is required for grr-la channel")
        return guerrillamail_mirrors.grrla_get_emails(token, email)
    elif channel == "grr-la-com":
        if not token:
            raise ValueError("token is required for grr-la-com channel")
        return guerrillamail_mirrors.grrla_com_get_emails(token, email)
    elif channel == "guerrillamail-info":
        if not token:
            raise ValueError("token is required for guerrillamail-info channel")
        return guerrillamail_mirrors.guerrillamail_info_get_emails(token, email)
    elif channel == "spam4me":
        if not token:
            raise ValueError("token is required for spam4me channel")
        return guerrillamail_mirrors.spam4me_get_emails(token, email)
    elif channel == "guerrillamail-net":
        if not token:
            raise ValueError("token is required for guerrillamail-net channel")
        return guerrillamail_mirrors.guerrillamail_net_get_emails(token, email)
    elif channel == "guerrillamail-org":
        if not token:
            raise ValueError("token is required for guerrillamail-org channel")
        return guerrillamail_mirrors.guerrillamail_org_get_emails(token, email)
    elif channel == "guerrillamailblock":
        if not token:
            raise ValueError("token is required for guerrillamailblock channel")
        return guerrillamail_mirrors.guerrillamailblock_get_emails(token, email)
    elif channel == "guerrillamail-com-www":
        if not token:
            raise ValueError("token is required for guerrillamail-com-www channel")
        return guerrillamail_mirrors.guerrillamail_com_www_get_emails(token, email)
    else:
        raise ValueError(f"Unknown channel: {channel}")


class TempEmailClient:
    """
    临时邮箱客户端
    封装了邮箱创建和邮件获取的完整流程，自动管理邮箱信息和认证令牌

    示例:
        client = TempEmailClient()
        info = client.generate(GenerateEmailOptions(channel="maildrop"))
        result = client.get_emails()
        if result.success:
            print(f"收到 {len(result.emails)} 封邮件")
    """

    def __init__(self):
        self._email_info: Optional[EmailInfo] = None

    def generate(self, options: Optional[GenerateEmailOptions] = None) -> Optional[EmailInfo]:
        """创建临时邮箱并缓存邮箱信息"""
        self._email_info = generate_email(options)
        return self._email_info

    def get_emails(self, options: Optional[GetEmailsOptions] = None) -> GetEmailsResult:
        """获取当前邮箱的邮件列表，必须先调用 generate()"""
        if self._email_info is None:
            report_telemetry("get_emails", "", False, 0, 0, "No email generated. Call generate() first")
            raise RuntimeError("No email generated. Call generate() first")

        return get_emails(self._email_info, options)

    @property
    def email_info(self) -> Optional[EmailInfo]:
        """获取当前缓存的邮箱信息"""
        return self._email_info
