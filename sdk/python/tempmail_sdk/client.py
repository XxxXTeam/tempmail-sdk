"""
SDK 主入口
聚合所有渠道的逻辑，提供统一的 API
"""

import random
import time
from typing import Optional, List
from .types import (
    EmailInfo,
    Email,
    GetEmailsResult,
    GenerateEmailOptions,
    GetEmailsOptions,
    ChannelInfo,
)
from .retry import with_retry, with_retry_with_attempts
from .telemetry import report_telemetry
from .logger import get_logger
from .backend_groups import (
    get_backend,
    is_backend_open,
    record_backend_failure,
    record_backend_success,
)
from .channel_domains import filter_channels_by_domain
from .registry import (
    CHANNEL_REGISTRY,
    CHANNEL_REGISTRY_MAP,
)

# 所有支持的公共渠道列表（有序），由注册表派生；随机尝试顺序在本端独立洗牌，不要求跨 SDK 一致
ALL_CHANNELS = [spec.channel for spec in CHANNEL_REGISTRY]

# 渠道信息映射表，由注册表派生
CHANNEL_INFO_MAP = {
    spec.channel: ChannelInfo(
        channel=spec.channel, name=spec.name, website=spec.website
    )
    for spec in CHANNEL_REGISTRY
}


def list_channels() -> List[ChannelInfo]:
    """获取所有支持的渠道列表"""
    return [CHANNEL_INFO_MAP[ch] for ch in ALL_CHANNELS]


def get_channel_info(channel: str) -> Optional[ChannelInfo]:
    """获取指定渠道的详细信息"""
    return CHANNEL_INFO_MAP.get(channel)


def generate_email(
    options: Optional[GenerateEmailOptions] = None,
) -> Optional[EmailInfo]:
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

    try_order = _build_channel_order(options.channel)

    # 解析域名筛选条件
    target_domains = []
    if options.suffix:
        s = options.suffix.lstrip("@")
        target_domains.append(s)
    if options.domains:
        target_domains.extend(options.domains)
    # 按域名筛选渠道
    if target_domains:
        try_order = filter_channels_by_domain(try_order, target_domains)

    max_channels = getattr(options, "max_channels_tried", 0) or 20
    total_timeout = getattr(options, "total_timeout", 0) or 60.0
    start_time = time.time()
    failed_backends: set = set()

    channels_tried = 0
    last_err = ""
    for ch in try_order:
        if channels_tried >= max_channels:
            logger.warning(f"已尝试最大渠道数 ({max_channels})，停止")
            break
        if time.time() - start_time >= total_timeout:
            logger.warning("整体超时，停止尝试")
            break

        backend = get_backend(ch)

        if backend:
            if backend in failed_backends:
                logger.debug(f"跳过渠道 {ch}，同后端 {backend} 已失败")
                continue
            if not is_backend_open(backend):
                logger.debug(f"跳过渠道 {ch}，后端 {backend} 熔断中")
                continue

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
            if backend:
                record_backend_success(backend)
            return result
        err = r.error
        last_err = str(err) if err else "unknown"
        logger.warning(f"渠道 {ch} 不可用: {last_err}，尝试下一个渠道")
        if backend:
            failed_backends.add(backend)
            record_backend_failure(backend)

    logger.error("所有渠道均不可用，创建邮箱失败")
    report_telemetry("generate_email", "", False, 0, channels_tried, last_err)
    return None


def _build_channel_order(preferred: Optional[str] = None) -> list:
    """
    构建渠道尝试顺序
    指定渠道时优先尝试该渠道，其余渠道打乱追加
    未指定时打乱全部渠道
    返回值是本端运行时随机顺序，不作为跨 SDK 一致性约束
    """
    shuffled = ALL_CHANNELS[:]
    random.shuffle(shuffled)
    if not preferred:
        return shuffled
    rest = [ch for ch in shuffled if ch != preferred]
    return [preferred] + rest


def _generate_email_once(channel: str, options: GenerateEmailOptions) -> EmailInfo:
    """单次创建邮箱（不含重试逻辑）"""
    spec = CHANNEL_REGISTRY_MAP.get(channel)
    if spec is None:
        raise ValueError(f"Unknown channel: {channel}")
    return spec.generate(options)


def get_emails(
    info: EmailInfo, options: Optional[GetEmailsOptions] = None
) -> GetEmailsResult:
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
        report_telemetry(
            "get_emails",
            "",
            False,
            0,
            0,
            "EmailInfo is required, call generate_email() first",
        )
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
        return GetEmailsResult(
            channel=channel, email=email, emails=emails, success=True
        )

    err = r.error
    err_s = str(err) if err else "unknown"
    logger.error(f"获取邮件失败, 渠道: {channel}, 错误: {err_s}")
    report_telemetry("get_emails", channel, False, r.attempts, 0, err_s)
    return GetEmailsResult(channel=channel, email=email, emails=[], success=False)


def _get_emails_once(channel: str, email: str, token: Optional[str]) -> List[Email]:
    """单次获取邮件（不含重试逻辑）"""
    spec = CHANNEL_REGISTRY_MAP.get(channel)
    if spec is None:
        raise ValueError(f"Unknown channel: {channel}")
    return spec.get_emails(email, token)


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

    def generate(
        self, options: Optional[GenerateEmailOptions] = None
    ) -> Optional[EmailInfo]:
        """创建临时邮箱并缓存邮箱信息"""
        self._email_info = generate_email(options)
        return self._email_info

    def get_emails(self, options: Optional[GetEmailsOptions] = None) -> GetEmailsResult:
        """获取当前邮箱的邮件列表，必须先调用 generate()"""
        if self._email_info is None:
            report_telemetry(
                "get_emails",
                "",
                False,
                0,
                0,
                "No email generated. Call generate() first",
            )
            raise RuntimeError("No email generated. Call generate() first")

        return get_emails(self._email_info, options)

    @property
    def email_info(self) -> Optional[EmailInfo]:
        """获取当前缓存的邮箱信息"""
        return self._email_info
