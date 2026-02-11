"""
共享 HTTP 客户端
所有 provider 通过此模块发起 HTTP 请求，自动应用全局配置（代理、超时、SSL 等）

性能优化：Session 内部缓存复用，仅在配置变更时重建，保证连接池复用
"""

import requests
from .config import get_config, get_config_version

# 缓存的 Session 及其对应的配置版本
_cached_session = None
_cached_version = -1


def get_session() -> requests.Session:
    """
    获取带全局配置的 requests.Session
    内部缓存复用，仅在配置变更时重建
    """
    global _cached_session, _cached_version

    current_version = get_config_version()
    if _cached_session is not None and _cached_version == current_version:
        return _cached_session

    # 配置已变更或首次创建，重建 Session
    config = get_config()
    session = requests.Session()

    # 代理
    if config.proxy:
        session.proxies = {
            "http": config.proxy,
            "https": config.proxy,
        }

    # SSL 验证
    session.verify = not config.insecure

    # 自定义请求头
    if config.headers:
        session.headers.update(config.headers)

    _cached_session = session
    _cached_version = current_version
    return session


def get(url: str, headers: dict = None, timeout: int = None, **kwargs) -> requests.Response:
    """带全局配置的 GET 请求"""
    config = get_config()
    session = get_session()
    effective_timeout = timeout if timeout is not None else config.timeout
    return session.get(url, headers=headers, timeout=effective_timeout, **kwargs)


def post(url: str, headers: dict = None, timeout: int = None, **kwargs) -> requests.Response:
    """带全局配置的 POST 请求"""
    config = get_config()
    session = get_session()
    effective_timeout = timeout if timeout is not None else config.timeout
    return session.post(url, headers=headers, timeout=effective_timeout, **kwargs)
