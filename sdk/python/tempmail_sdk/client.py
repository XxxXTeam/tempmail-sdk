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
from .retry import with_retry
from .logger import get_logger
from .providers import (
    tempmail, linshi_email, linshiyou, tempmail_lol, chatgpt_org_uk,
    temp_mail_io, awamail, temporary_email_org, mail_tm,
    dropmail, guerrillamail, maildrop, smail_pw,
    boomlify, minmail, vip_215, anonbox, fake_legal,
)

# 所有支持的渠道列表
ALL_CHANNELS = [
    "tempmail", "linshi-email", "linshiyou", "tempmail-lol", "chatgpt-org-uk",
    "temp-mail-io", "awamail", "temporary-email-org", "mail-tm",
    "dropmail", "guerrillamail", "maildrop", "smail-pw",
    "boomlify", "minmail", "vip-215", "anonbox", "fake-legal",
]

# 渠道信息映射表
CHANNEL_INFO_MAP = {
    "tempmail": ChannelInfo(channel="tempmail", name="TempMail", website="tempmail.ing"),
    "linshi-email": ChannelInfo(channel="linshi-email", name="临时邮箱", website="linshi-email.com"),
    "linshiyou": ChannelInfo(channel="linshiyou", name="临时邮", website="linshiyou.com"),
    "tempmail-lol": ChannelInfo(channel="tempmail-lol", name="TempMail LOL", website="tempmail.lol"),
    "chatgpt-org-uk": ChannelInfo(channel="chatgpt-org-uk", name="ChatGPT Mail", website="mail.chatgpt.org.uk"),
    "temp-mail-io": ChannelInfo(channel="temp-mail-io", name="Temp Mail IO", website="temp-mail.io"),
    "awamail": ChannelInfo(channel="awamail", name="AwaMail", website="awamail.com"),
    "temporary-email-org": ChannelInfo(channel="temporary-email-org", name="Temporary Email", website="temporary-email.org"),
    "mail-tm": ChannelInfo(channel="mail-tm", name="Mail.tm", website="mail.tm"),
    "dropmail": ChannelInfo(channel="dropmail", name="DropMail", website="dropmail.me"),
    "guerrillamail": ChannelInfo(channel="guerrillamail", name="Guerrilla Mail", website="guerrillamail.com"),
    "maildrop": ChannelInfo(channel="maildrop", name="Maildrop", website="maildrop.cx"),
    "smail-pw": ChannelInfo(channel="smail-pw", name="Smail.pw", website="smail.pw"),
    "boomlify": ChannelInfo(channel="boomlify", name="Boomlify", website="boomlify.com"),
    "minmail": ChannelInfo(channel="minmail", name="MinMail", website="minmail.app"),
    "vip-215": ChannelInfo(channel="vip-215", name="VIP 215", website="vip.215.im"),
    "anonbox": ChannelInfo(channel="anonbox", name="Anonbox", website="anonbox.net"),
    "fake-legal": ChannelInfo(channel="fake-legal", name="Fake Legal", website="fake.legal"),
}


def list_channels() -> List[ChannelInfo]:
    """获取所有支持的渠道列表"""
    return [CHANNEL_INFO_MAP[ch] for ch in ALL_CHANNELS]


def get_channel_info(channel: str) -> Optional[ChannelInfo]:
    """获取指定渠道的详细信息"""
    return CHANNEL_INFO_MAP.get(channel)


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

    for ch in try_order:
        logger.info(f"创建临时邮箱, 渠道: {ch}")
        try:
            result = with_retry(
                lambda c=ch: _generate_email_once(c, options),
                options.retry,
            )
            logger.info(f"邮箱创建成功: {result.email} (渠道: {ch})")
            return result
        except Exception as e:
            logger.warning(f"渠道 {ch} 不可用: {e}，尝试下一个渠道")

    logger.error("所有渠道均不可用，创建邮箱失败")
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
    elif channel == "linshi-email":
        return linshi_email.generate_email()
    elif channel == "linshiyou":
        return linshiyou.generate_email()
    elif channel == "tempmail-lol":
        return tempmail_lol.generate_email(options.domain)
    elif channel == "chatgpt-org-uk":
        return chatgpt_org_uk.generate_email()
    elif channel == "temp-mail-io":
        return temp_mail_io.generate_email()
    elif channel == "awamail":
        return awamail.generate_email()
    elif channel == "temporary-email-org":
        return temporary_email_org.generate_email()
    elif channel == "mail-tm":
        return mail_tm.generate_email()
    elif channel == "dropmail":
        return dropmail.generate_email()
    elif channel == "guerrillamail":
        return guerrillamail.generate_email()
    elif channel == "maildrop":
        return maildrop.generate_email(options.domain)
    elif channel == "smail-pw":
        return smail_pw.generate_email()
    elif channel == "boomlify":
        return boomlify.generate_email()
    elif channel == "minmail":
        return minmail.generate_email()
    elif channel == "vip-215":
        return vip_215.generate_email()
    elif channel == "anonbox":
        return anonbox.generate_email()
    elif channel == "fake-legal":
        return fake_legal.generate_email(options.domain)
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
        raise ValueError("EmailInfo is required, call generate_email() first")

    channel = info.channel
    email = info.email
    token = info._token
    retry = options.retry if options else None
    logger = get_logger()

    if not channel:
        raise ValueError("channel is required")
    if not email and channel != "tempmail-lol":
        raise ValueError("email is required")

    logger.debug(f"获取邮件, 渠道: {channel}, 邮箱: {email}")

    try:
        emails = with_retry(
            lambda: _get_emails_once(channel, email, token),
            retry,
        )
        if emails:
            logger.info(f"获取到 {len(emails)} 封邮件, 渠道: {channel}")
        else:
            logger.debug(f"暂无邮件, 渠道: {channel}")

        return GetEmailsResult(channel=channel, email=email, emails=emails, success=True)
    except Exception as e:
        logger.error(f"获取邮件失败, 渠道: {channel}, 错误: {e}")
        return GetEmailsResult(channel=channel, email=email, emails=[], success=False)


def _get_emails_once(channel: str, email: str, token: Optional[str]) -> List[Email]:
    """单次获取邮件（不含重试逻辑）"""
    if channel == "tempmail":
        return tempmail.get_emails(email)
    elif channel == "linshi-email":
        if not token:
            raise ValueError("token is required for linshi-email channel")
        return linshi_email.get_emails(email, token)
    elif channel == "linshiyou":
        if not token:
            raise ValueError("token is required for linshiyou channel")
        return linshiyou.get_emails(token, email)
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
    elif channel == "awamail":
        if not token:
            raise ValueError("token is required for awamail channel")
        return awamail.get_emails(token, email)
    elif channel == "temporary-email-org":
        if not token:
            raise ValueError("token is required for temporary-email-org channel")
        return temporary_email_org.get_emails(token, email)
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
    elif channel == "maildrop":
        if not token:
            raise ValueError("token is required for maildrop channel")
        return maildrop.get_emails(token, email)
    elif channel == "smail-pw":
        if not token:
            raise ValueError("token is required for smail-pw channel")
        return smail_pw.get_emails(token, email)
    elif channel == "boomlify":
        return boomlify.get_emails(email)
    elif channel == "minmail":
        if not token:
            raise ValueError("token is required for minmail channel")
        return minmail.get_emails(email, token)
    elif channel == "vip-215":
        if not token:
            raise ValueError("token is required for vip-215 channel")
        return vip_215.get_emails(token, email)
    elif channel == "anonbox":
        if not token:
            raise ValueError("token is required for anonbox channel")
        return anonbox.get_emails(token, email)
    elif channel == "fake-legal":
        return fake_legal.get_emails(email)
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

    def generate(self, options: Optional[GenerateEmailOptions] = None) -> EmailInfo:
        """创建临时邮箱并缓存邮箱信息"""
        self._email_info = generate_email(options)
        return self._email_info

    def get_emails(self, options: Optional[GetEmailsOptions] = None) -> GetEmailsResult:
        """获取当前邮箱的邮件列表，必须先调用 generate()"""
        if self._email_info is None:
            raise RuntimeError("No email generated. Call generate() first")

        return get_emails(self._email_info, options)

    @property
    def email_info(self) -> Optional[EmailInfo]:
        """获取当前缓存的邮箱信息"""
        return self._email_info
