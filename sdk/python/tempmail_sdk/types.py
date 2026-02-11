"""
SDK 类型定义
"""

from dataclasses import dataclass, field
from typing import Optional, List, Literal, Any

# 支持的临时邮箱渠道标识，每个渠道对应一个第三方临时邮箱服务商
Channel = Literal[
    "tempmail",
    "linshi-email",
    "tempmail-lol",
    "chatgpt-org-uk",
    "tempmail-la",
    "temp-mail-io",
    "awamail",
    "mail-tm",
    "dropmail",
    "guerrillamail",
    "maildrop",
]


@dataclass
class EmailInfo:
    """创建临时邮箱后返回的邮箱信息"""

    channel: str      # 创建该邮箱所使用的渠道
    email: str        # 临时邮箱地址
    token: Optional[str] = None       # 认证令牌，部分渠道获取邮件时需要
    expires_at: Optional[int] = None  # 邮箱过期时间（毫秒时间戳）
    created_at: Optional[Any] = None  # 邮箱创建时间


@dataclass
class EmailAttachment:
    """邮件附件信息"""

    filename: str = ""                    # 附件文件名
    size: Optional[int] = None            # 附件大小（字节）
    content_type: Optional[str] = None    # 附件 MIME 类型
    url: Optional[str] = None             # 附件下载地址


@dataclass
class Email:
    """标准化的邮件对象"""

    id: str = ""           # 邮件唯一标识
    from_addr: str = ""    # 发件人地址
    to: str = ""           # 收件人地址
    subject: str = ""      # 邮件主题
    text: str = ""         # 纯文本内容
    html: str = ""         # HTML 内容
    date: str = ""         # 接收日期（ISO 8601 格式）
    is_read: bool = False  # 是否已读
    attachments: List[EmailAttachment] = field(default_factory=list)  # 附件列表


@dataclass
class GetEmailsResult:
    """获取邮件列表的结果"""

    channel: str = ""     # 渠道标识
    email: str = ""       # 邮箱地址
    emails: List[Email] = field(default_factory=list)  # 邮件列表
    success: bool = True  # 本次请求是否成功


@dataclass
class RetryConfig:
    """重试配置"""

    max_retries: int = 2        # 最大重试次数（不含首次请求），默认 2
    initial_delay: float = 1.0  # 初始重试延迟（秒），默认 1.0
    max_delay: float = 5.0      # 最大重试延迟（秒），默认 5.0
    timeout: float = 15.0       # 请求超时（秒），默认 15.0


@dataclass
class GenerateEmailOptions:
    """创建临时邮箱的选项"""

    channel: Optional[str] = None           # 指定渠道，不指定则随机选择
    duration: int = 30                      # tempmail 渠道的有效期（分钟）
    domain: Optional[str] = None            # tempmail-lol 渠道的指定域名
    retry: Optional[RetryConfig] = None     # 重试配置


@dataclass
class GetEmailsOptions:
    """获取邮件的选项"""

    channel: str = ""                       # 渠道标识（必需）
    email: str = ""                         # 邮箱地址（必需）
    token: Optional[str] = None             # 认证令牌（部分渠道必需）
    retry: Optional[RetryConfig] = None     # 重试配置


@dataclass
class ChannelInfo:
    """渠道信息"""

    channel: str = ""   # 渠道标识
    name: str = ""      # 渠道显示名称
    website: str = ""   # 对应的临时邮箱服务网站
