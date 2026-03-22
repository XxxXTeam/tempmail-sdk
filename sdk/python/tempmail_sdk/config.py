"""
SDK 全局配置
包含代理、超时、SSL 等设置，作用于所有 HTTP 请求

支持环境变量自动读取（优先级低于代码设置）：
  TEMPMAIL_PROXY    - 代理 URL
  TEMPMAIL_TIMEOUT  - 超时秒数
  TEMPMAIL_INSECURE - 设为 "1" 或 "true" 跳过 SSL 验证
  DROPMAIL_AUTH_TOKEN / DROPMAIL_API_TOKEN - DropMail af_ 令牌（可选；未设置则自动 generate/renew）
  DROPMAIL_NO_AUTO_TOKEN - 禁止自动拉取/续期
  DROPMAIL_RENEW_LIFETIME - renew 的 lifetime，默认 1d
"""

import os
from dataclasses import dataclass, field
from typing import Optional, Dict


@dataclass
class SDKConfig:
    """SDK 全局配置"""
    proxy: Optional[str] = None
    """代理 URL，支持 http/https/socks5，如 "http://127.0.0.1:7890" """
    timeout: int = 15
    """全局默认超时秒数"""
    insecure: bool = False
    """跳过 SSL 证书验证（调试用）"""
    headers: Optional[Dict[str, str]] = None
    """自定义请求头，会合并到每个请求中"""
    dropmail_auth_token: Optional[str] = None
    """DropMail GraphQL 路径中的 af_ 令牌；空则自动申请"""
    dropmail_disable_auto_token: bool = False
    """为 True 时不自动 generate/renew（须配置令牌）"""
    dropmail_renew_lifetime: Optional[str] = None
    """传给 /api/token/renew 的 lifetime，默认 1d"""


def _load_env_config() -> SDKConfig:
    """从环境变量读取默认配置"""
    proxy = os.environ.get("TEMPMAIL_PROXY") or None
    timeout_str = os.environ.get("TEMPMAIL_TIMEOUT")
    timeout = int(timeout_str) if timeout_str and timeout_str.isdigit() else 15
    insecure_val = os.environ.get("TEMPMAIL_INSECURE", "")
    insecure = insecure_val in ("1", "true", "True", "TRUE")
    dm_tok = (os.environ.get("DROPMAIL_AUTH_TOKEN") or os.environ.get("DROPMAIL_API_TOKEN") or "").strip() or None
    dm_no = (os.environ.get("DROPMAIL_NO_AUTO_TOKEN") or "").strip().lower() in ("1", "true", "yes")
    dm_renew = (os.environ.get("DROPMAIL_RENEW_LIFETIME") or "").strip() or None
    return SDKConfig(
        proxy=proxy,
        timeout=timeout,
        insecure=insecure,
        dropmail_auth_token=dm_tok,
        dropmail_disable_auto_token=dm_no,
        dropmail_renew_lifetime=dm_renew,
    )


_global_config = _load_env_config()
_config_version = 0


def set_config(config: Optional[SDKConfig] = None, **kwargs) -> None:
    """
    设置 SDK 全局配置
    设置后自动使已缓存的 HTTP Session 失效，下次请求时按新配置重建

    可以传入 SDKConfig 对象，也可以用关键字参数：
        set_config(proxy="http://127.0.0.1:7890", timeout=30)
        set_config(insecure=True)
        set_config(SDKConfig(proxy="socks5://127.0.0.1:1080"))
    """
    global _global_config, _config_version
    if config is not None:
        _global_config = config
    else:
        _global_config = SDKConfig(**kwargs)
    _config_version += 1


def get_config() -> SDKConfig:
    """获取当前 SDK 全局配置"""
    return _global_config


def get_config_version() -> int:
    """获取当前配置版本号，用于缓存失效判断"""
    return _config_version
