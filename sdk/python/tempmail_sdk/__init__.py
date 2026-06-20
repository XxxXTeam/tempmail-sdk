"""
临时邮箱 SDK (Python)
支持 55 个邮箱服务提供商，所有渠道返回统一标准化格式
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
from .retry import with_retry, with_retry_with_attempts, RetryAttemptsResult
from .normalize import normalize_email
from .config import SDKConfig, set_config, get_config
from .telemetry import get_sdk_version

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
    "with_retry_with_attempts",
    "RetryAttemptsResult",
    "normalize_email",
    "SDKConfig",
    "set_config",
    "get_config",
    "get_sdk_version",
]
