"""
临时邮箱 SDK (Python)
支持 268 个 channel 标识，按 100 个独立服务商归并，所有渠道返回统一标准化格式
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
from .logger import (
    set_log_level,
    set_logger,
    get_logger,
    LOG_SILENT,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
)
from .retry import with_retry, with_retry_with_attempts, RetryAttemptsResult
from .normalize import normalize_email
from .config import SDKConfig, set_config, get_config
from .telemetry import get_sdk_version
from .webui import start_webui, stop_webui, _auto_start_from_env

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
    "start_webui",
    "stop_webui",
]

# 模块加载时按环境变量 TEMPMAIL_WEBUI 自动启动 WebUI（默认不启动）
_auto_start_from_env()
