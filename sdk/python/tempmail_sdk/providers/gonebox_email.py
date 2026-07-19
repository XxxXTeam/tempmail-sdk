"""
gonebox-email 渠道 — https://gonebox.email

一次性临时邮箱服务，无需认证。
  1. 创建: POST https://api.gonebox.email/api/v1/inboxes body: {"domain":"gonebox.email"}
     响应: {"success":true,"data":{"id":"...","address":"...","domain":"...","expiresAt":...,"ttl":...}}
  2. 读信: GET https://api.gonebox.email/api/v1/inboxes/{address}/messages
     响应: {"success":true,"data":{"address":"...","count":0,"messages":[...]}}
"""

from typing import List

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "gonebox-email"
BASE_URL = "https://api.gonebox.email/api/v1"

_HEADERS = {
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36"
    ),
    "Accept": "application/json",
    "Content-Type": "application/json",
}


def generate_email() -> EmailInfo:
    """
    创建 gonebox.email 临时邮箱
    POST /inboxes 请求分配一个随机邮箱地址，无需认证
    """
    resp = tm_http.post(
        f"{BASE_URL}/inboxes",
        headers=_HEADERS,
        json={"domain": "gonebox.email"},
        timeout=15,
    )
    resp.raise_for_status()
    body = resp.json()

    if not isinstance(body, dict) or not body.get("success"):
        raise RuntimeError(f"gonebox-email: 创建邮箱失败，响应: {body!r}")

    data = body.get("data")
    if not isinstance(data, dict):
        raise RuntimeError(f"gonebox-email: 响应 data 格式无效，响应: {body!r}")

    address = data.get("address", "")
    if not address:
        raise RuntimeError(f"gonebox-email: 缺少邮箱地址，响应: {body!r}")

    # expiresAt 为秒级时间戳，转为毫秒
    expires_at = None
    raw_expires = data.get("expiresAt")
    if raw_expires:
        try:
            expires_at = int(raw_expires) * 1000
        except (ValueError, TypeError):
            pass

    return EmailInfo(
        channel=CHANNEL,
        email=address,
        _token="",
        expires_at=expires_at,
    )


def get_emails(token: str, email: str) -> List[Email]:
    """
    获取 gonebox.email 邮件列表
    GET /inboxes/{address}/messages，无需认证
    """
    address = (email or "").strip()
    if not address:
        raise ValueError("gonebox-email: 缺少邮箱地址")

    headers = {k: v for k, v in _HEADERS.items() if k != "Content-Type"}

    resp = tm_http.get(
        f"{BASE_URL}/inboxes/{address}/messages",
        headers=headers,
        timeout=15,
    )
    if resp.status_code == 404:
        return []
    resp.raise_for_status()

    body = resp.json()
    if not isinstance(body, dict) or not body.get("success"):
        return []

    data = body.get("data")
    if not isinstance(data, dict):
        return []

    messages = data.get("messages")
    if not isinstance(messages, list) or not messages:
        return []

    out: List[Email] = []
    for msg in messages:
        if not isinstance(msg, dict):
            continue
        raw = {
            "id": msg.get("id", ""),
            "from": msg.get("from", "") or msg.get("sender", ""),
            "to": address,
            "subject": msg.get("subject", ""),
            "text": msg.get("text", "") or msg.get("body_plain", ""),
            "html": msg.get("html", "") or msg.get("body_html", ""),
            "date": str(msg.get("date", "") or msg.get("received_at", "")),
            "is_read": False,
            "attachments": [],
        }
        out.append(normalize_email(raw, address))

    return out
