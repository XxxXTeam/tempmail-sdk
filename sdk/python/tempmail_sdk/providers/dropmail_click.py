"""
dropmail-click 渠道 — https://dropmail.click

独立后端临时邮箱，免注册、无鉴权。Token 即邮箱地址本身。
流程:
  1. POST /api/v1/public/mailbox → {address, created_at, expires_at}，有效期约 10 分钟
  2. GET /api/v1/public/mailbox/{email} → {messages: [{id, address, from, subject, text, html}]}
"""

from typing import Any, Dict, List
from urllib.parse import quote

from ..config import get_config
from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "dropmail-click"
BASE_URL = "https://dropmail.click"


def _get_ua() -> str:
    """获取 User-Agent，优先使用配置中的自定义 UA"""
    c = get_config()
    return (c.headers or {}).get("User-Agent") or (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36"
    )


def _first(msg: Dict[str, Any], *keys: str) -> str:
    """按优先级从消息字典中提取首个非空值并转为字符串"""
    for key in keys:
        val = msg.get(key)
        if val is not None and val != "":
            return str(val)
    return ""


def generate_email() -> EmailInfo:
    """
    创建 dropmail.click 临时邮箱
    POST /api/v1/public/mailbox → {address, created_at, expires_at}；token = 邮箱地址
    """
    resp = tm_http.post(
        f"{BASE_URL}/api/v1/public/mailbox",
        headers={"User-Agent": _get_ua(), "Accept": "application/json"},
    )
    resp.raise_for_status()

    data = resp.json()
    if not isinstance(data, dict):
        raise ValueError("dropmail-click: 无效响应")
    address = data.get("address")
    if not address:
        raise ValueError("dropmail-click: 无效响应，缺少 address")
    return EmailInfo(
        channel=CHANNEL,
        email=str(address),
        _token=str(address),
        expires_at=data.get("expires_at"),
    )


def get_emails(email: str) -> List[Email]:
    """
    获取 dropmail.click 邮件列表
    GET /api/v1/public/mailbox/{email} → {messages: [{id, address, from, subject, text, html}]}
    """
    addr = (email or "").strip()
    if not addr:
        raise ValueError("dropmail-click: 缺少邮箱地址")

    resp = tm_http.get(
        f"{BASE_URL}/api/v1/public/mailbox/{quote(addr, safe='')}",
        headers={"User-Agent": _get_ua(), "Accept": "application/json"},
    )
    resp.raise_for_status()

    data = resp.json()
    if not isinstance(data, dict):
        return []
    messages = data.get("messages")
    if not isinstance(messages, list):
        return []

    out: List[Email] = []
    for msg in messages:
        if not isinstance(msg, dict):
            continue
        row = {
            "id": _first(msg, "id"),
            "from": _first(msg, "from"),
            "to": _first(msg, "address") or addr,
            "subject": _first(msg, "subject"),
            "text": _first(msg, "text"),
            "html": _first(msg, "html"),
            "date": _first(msg, "received_at", "date"),
        }
        out.append(normalize_email(row, addr))
    return out
