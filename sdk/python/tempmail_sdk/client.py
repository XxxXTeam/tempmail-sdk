"""
SDK 主入口
聚合所有渠道的逻辑，提供统一的 API
"""

import random
from typing import Optional, List
from .types import (
    EmailInfo, Email, GetEmailsResult,
    GenerateEmailOptions, GetEmailsOptions,
    ChannelInfo, RetryConfig,
)
from .retry import with_retry
from .logger import get_logger
from .providers import (
    tempmail, linshi_email, tempmail_lol, chatgpt_org_uk,
    tempmail_la, temp_mail_io, awamail, mail_tm,
    dropmail, guerrillamail, maildrop,
)

# 所有支持的渠道列表
ALL_CHANNELS = [
    "tempmail", "linshi-email", "tempmail-lol", "chatgpt-org-uk",
    "tempmail-la", "temp-mail-io", "awamail", "mail-tm",
    "dropmail", "guerrillamail", "maildrop",
]

# 渠道信息映射表
CHANNEL_INFO_MAP = {
    "tempmail": ChannelInfo(channel="tempmail", name="TempMail", website="tempmail.ing"),
    "linshi-email": ChannelInfo(channel="linshi-email", name="临时邮箱", website="linshi-email.com"),
    "tempmail-lol": ChannelInfo(channel="tempmail-lol", name="TempMail LOL", website="tempmail.lol"),
    "chatgpt-org-uk": ChannelInfo(channel="chatgpt-org-uk", name="ChatGPT Mail", website="mail.chatgpt.org.uk"),
    "tempmail-la": ChannelInfo(channel="tempmail-la", name="TempMail LA", website="tempmail.la"),
    "temp-mail-io": ChannelInfo(channel="temp-mail-io", name="Temp Mail IO", website="temp-mail.io"),
    "awamail": ChannelInfo(channel="awamail", name="AwaMail", website="awamail.com"),
    "mail-tm": ChannelInfo(channel="mail-tm", name="Mail.tm", website="mail.tm"),
    "dropmail": ChannelInfo(channel="dropmail", name="DropMail", website="dropmail.me"),
    "guerrillamail": ChannelInfo(channel="guerrillamail", name="Guerrilla Mail", website="guerrillamail.com"),
    "maildrop": ChannelInfo(channel="maildrop", name="Maildrop", website="maildrop.cc"),
}


def list_channels() -> List[ChannelInfo]:
    """获取所有支持的渠道列表"""
    return [CHANNEL_INFO_MAP[ch] for ch in ALL_CHANNELS]


def get_channel_info(channel: str) -> Optional[ChannelInfo]:
    """获取指定渠道的详细信息"""
    return CHANNEL_INFO_MAP.get(channel)


def generate_email(options: Optional[GenerateEmailOptions] = None) -> EmailInfo:
    """
    创建临时邮箱

    错误处理策略:
    - 网络错误、超时、HTTP 4xx/5xx → 自动重试（默认 2 次，指数退避）
    - 重试耗尽后仍失败时抛出异常

    示例:
        info = generate_email(GenerateEmailOptions(channel="guerrillamail"))
        print(info.email)
    """
    if options is None:
        options = GenerateEmailOptions()

    channel = options.channel or random.choice(ALL_CHANNELS)
    logger = get_logger()

    logger.info(f"创建临时邮箱, 渠道: {channel}")

    result = with_retry(
        lambda: _generate_email_once(channel, options),
        options.retry,
    )

    logger.info(f"邮箱创建成功: {result.email}")
    return result


def _generate_email_once(channel: str, options: GenerateEmailOptions) -> EmailInfo:
    """单次创建邮箱（不含重试逻辑）"""
    if channel == "tempmail":
        return tempmail.generate_email(options.duration)
    elif channel == "linshi-email":
        return linshi_email.generate_email()
    elif channel == "tempmail-lol":
        return tempmail_lol.generate_email(options.domain)
    elif channel == "chatgpt-org-uk":
        return chatgpt_org_uk.generate_email()
    elif channel == "tempmail-la":
        return tempmail_la.generate_email()
    elif channel == "temp-mail-io":
        return temp_mail_io.generate_email()
    elif channel == "awamail":
        return awamail.generate_email()
    elif channel == "mail-tm":
        return mail_tm.generate_email()
    elif channel == "dropmail":
        return dropmail.generate_email()
    elif channel == "guerrillamail":
        return guerrillamail.generate_email()
    elif channel == "maildrop":
        return maildrop.generate_email()
    else:
        raise ValueError(f"Unknown channel: {channel}")


def get_emails(options: GetEmailsOptions) -> GetEmailsResult:
    """
    获取邮件列表

    错误处理策略:
    - 网络错误、超时、HTTP 4xx/5xx → 自动重试（默认 2 次）
    - 重试耗尽后返回 GetEmailsResult(success=False, emails=[])
    - 参数校验错误直接抛出异常

    示例:
        result = get_emails(GetEmailsOptions(
            channel="guerrillamail",
            email="xxx@guerrillamailblock.com",
            token="sid_token_value",
        ))
        if result.success:
            print(f"收到 {len(result.emails)} 封邮件")
    """
    channel = options.channel
    email = options.email
    token = options.token
    logger = get_logger()

    if not channel:
        raise ValueError("channel is required")
    if not email and channel != "tempmail-lol":
        raise ValueError("email is required")

    logger.debug(f"获取邮件, 渠道: {channel}, 邮箱: {email}")

    try:
        emails = with_retry(
            lambda: _get_emails_once(channel, email, token),
            options.retry,
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
        return linshi_email.get_emails(email)
    elif channel == "tempmail-lol":
        if not token:
            raise ValueError("token is required for tempmail-lol channel")
        return tempmail_lol.get_emails(token, email)
    elif channel == "chatgpt-org-uk":
        return chatgpt_org_uk.get_emails(email)
    elif channel == "tempmail-la":
        return tempmail_la.get_emails(email)
    elif channel == "temp-mail-io":
        return temp_mail_io.get_emails(email)
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
    elif channel == "maildrop":
        if not token:
            raise ValueError("token is required for maildrop channel")
        return maildrop.get_emails(token, email)
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

        if options is None:
            options = GetEmailsOptions(
                channel=self._email_info.channel,
                email=self._email_info.email,
                token=self._email_info.token,
            )

        return get_emails(options)

    @property
    def email_info(self) -> Optional[EmailInfo]:
        """获取当前缓存的邮箱信息"""
        return self._email_info
