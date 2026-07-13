"""
通用重试工具
提供请求重试、超时控制、指数退避等错误恢复机制
"""

import time
import requests
from dataclasses import dataclass
from typing import TypeVar, Callable, Optional, Generic
from .types import RetryConfig
from .logger import get_logger

T = TypeVar("T")

DEFAULT_CONFIG = RetryConfig()


@dataclass
class RetryAttemptsResult(Generic[T]):
    """with_retry_with_attempts 的返回值：ok 时 value 有效，否则 error 有效"""

    ok: bool
    attempts: int
    value: Optional[T] = None
    error: Optional[BaseException] = None


def _should_retry(error: Exception) -> bool:
    """
    判断错误是否应该重试
    以下错误类型会触发重试：
    - 网络连接错误（connection, timeout, dns, refused, reset 等）
    - HTTP 429 限流
    - HTTP 4xx/5xx 服务端错误（含状态码的错误消息）
    仅 SDK 内部参数校验类错误不重试
    """
    msg = str(error).lower()

    # 网络级别错误
    network_keywords = [
        "connection",
        "timeout",
        "timed out",
        "dns",
        "eof",
        "broken pipe",
        "network is unreachable",
        "refused",
        "reset",
    ]
    for kw in network_keywords:
        if kw in msg:
            return True

    # HTTP 429 限流
    if "429" in msg or "too many requests" in msg or "rate limit" in msg:
        return True

    # HTTP 4xx/5xx 错误（含状态码的错误消息）
    if ": 4" in msg or ": 5" in msg:
        return True
    if isinstance(error, requests.exceptions.HTTPError):
        return True

    return False


def with_retry_with_attempts(
    fn: Callable[[], T], config: Optional[RetryConfig] = None
) -> RetryAttemptsResult[T]:
    """
    与 with_retry 相同逻辑，返回是否成功、尝试次数；失败时不抛异常，见 error 字段。
    """
    cfg = config or DEFAULT_CONFIG
    logger = get_logger()
    last_error: Optional[Exception] = None

    for attempt in range(cfg.max_retries + 1):
        attempts = attempt + 1
        try:
            result = fn()
            if attempt > 0:
                logger.info(f"第 {attempt + 1} 次尝试成功")
            return RetryAttemptsResult(ok=True, attempts=attempts, value=result)
        except Exception as e:
            last_error = e
            error_msg = str(e)

            if attempt >= cfg.max_retries or not _should_retry(e):
                if attempt >= cfg.max_retries and cfg.max_retries > 0:
                    logger.error(f"重试 {cfg.max_retries} 次后仍失败: {error_msg}")
                elif not _should_retry(e):
                    logger.debug(f"不可重试的错误: {error_msg}")
                return RetryAttemptsResult(ok=False, attempts=attempts, error=e)

            delay = min(cfg.initial_delay * (2**attempt), cfg.max_delay)
            logger.warning(
                f"请求失败 ({error_msg})，{delay:.1f}s 后第 {attempt + 2} 次重试..."
            )
            time.sleep(delay)

    return RetryAttemptsResult(ok=False, attempts=cfg.max_retries + 1, error=last_error)


def with_retry(fn: Callable[[], T], config: Optional[RetryConfig] = None) -> T:
    """
    带重试的操作执行器

    功能:
    - 自动重试可恢复的错误（网络错误、超时、HTTP 4xx/5xx 等）
    - 指数退避策略避免短时间内过度请求
    - 不可恢复的错误（SDK 内部参数校验错误等）直接抛出

    参数:
        fn:     要执行的操作函数
        config: 重试配置，None 则使用默认值
    """
    r = with_retry_with_attempts(fn, config)
    if r.ok:
        return r.value  # type: ignore[return-value]
    raise r.error  # type: ignore[misc]
