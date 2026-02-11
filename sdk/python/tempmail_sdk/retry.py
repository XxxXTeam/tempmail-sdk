"""
通用重试工具
提供请求重试、超时控制、指数退避等错误恢复机制
"""

import time
import requests
from typing import TypeVar, Callable, Optional
from .types import RetryConfig
from .logger import get_logger

T = TypeVar("T")

DEFAULT_CONFIG = RetryConfig()


def _should_retry(error: Exception) -> bool:
    """
    判断错误是否应该重试
    网络错误、超时、HTTP 4xx/5xx 均可重试
    仅参数校验类错误不重试
    """
    msg = str(error).lower()

    # 网络级别错误
    network_keywords = [
        "connection", "timeout", "timed out",
        "dns", "eof", "broken pipe",
        "network is unreachable", "refused",
        "reset",
    ]
    for kw in network_keywords:
        if kw in msg:
            return True

    # HTTP 4xx/5xx 错误
    if ": 4" in msg or ": 5" in msg:
        return True
    if isinstance(error, requests.exceptions.HTTPError):
        return True

    return False


def with_retry(fn: Callable[[], T], config: Optional[RetryConfig] = None) -> T:
    """
    带重试的操作执行器

    功能:
    - 自动重试可恢复的错误（网络错误、超时、HTTP 4xx/5xx）
    - 指数退避策略避免短时间内过度请求
    - 不可恢复的错误直接抛出

    参数:
        fn:     要执行的操作函数
        config: 重试配置，None 则使用默认值
    """
    cfg = config or DEFAULT_CONFIG
    logger = get_logger()
    last_error: Optional[Exception] = None

    for attempt in range(cfg.max_retries + 1):
        try:
            result = fn()
            if attempt > 0:
                logger.info(f"第 {attempt + 1} 次尝试成功")
            return result
        except Exception as e:
            last_error = e
            error_msg = str(e)

            if attempt >= cfg.max_retries or not _should_retry(e):
                if attempt >= cfg.max_retries and cfg.max_retries > 0:
                    logger.error(f"重试 {cfg.max_retries} 次后仍失败: {error_msg}")
                elif not _should_retry(e):
                    logger.debug(f"不可重试的错误: {error_msg}")
                raise

            delay = min(cfg.initial_delay * (2 ** attempt), cfg.max_delay)
            logger.warning(f"请求失败 ({error_msg})，{delay:.1f}s 后第 {attempt + 2} 次重试...")
            time.sleep(delay)

    raise last_error  # type: ignore
