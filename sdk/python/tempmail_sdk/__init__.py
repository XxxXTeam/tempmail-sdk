"""
临时邮箱 SDK (Python)
支持 11 个邮箱服务提供商，所有渠道返回统一标准化格式
"""

from .types import (
    Channel,
    EmailInfo,
    Email,
    EmailAttachment,
    GetEmailsResult,
    GenerateEmailOptions,
    GetEmailsOptions,
    RetryConfig,
    ChannelInfo,
)
from .client import (
    generate_email,
    get_emails,
    list_channels,
    get_channel_info,
    TempEmailClient,
)
from .logger import set_log_level, set_logger, get_logger, LOG_SILENT, LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERROR
from .retry import with_retry
from .normalize import normalize_email

__all__ = [
    "Channel",
    "EmailInfo",
    "Email",
    "EmailAttachment",
    "GetEmailsResult",
    "GenerateEmailOptions",
    "GetEmailsOptions",
    "RetryConfig",
    "ChannelInfo",
    "generate_email",
    "get_emails",
    "list_channels",
    "get_channel_info",
    "TempEmailClient",
    "set_log_level",
    "set_logger",
    "get_logger",
    "LOG_SILENT",
    "LOG_DEBUG",
    "LOG_INFO",
    "LOG_WARNING",
    "LOG_ERROR",
    "with_retry",
    "normalize_email",
]
