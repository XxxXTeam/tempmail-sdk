"""
mailcat-ai 渠道 — https://mailcat.ai

AI 驱动的临时邮箱服务，创建时返回 Bearer token，后续读信需携带。
  1. 创建: POST https://api.mailcat.ai/mailboxes（无 body）
     响应: {"data":{"email":"...","token":"..."}}
  2. 读信: GET https://api.mailcat.ai/inbox
     请求头: Authorization: Bearer {token}
     响应: {"data":[...],"meta":{"mailbox":"...","unread":0,...}}
"""

from typing import List

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "mailcat-ai"
BASE_URL = "https://api.mailcat.ai"

_HEADERS = {
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36"
    ),
    "Accept": "application/json",
}


def generate_email() -> EmailInfo:
    """
    创建 mailcat.ai 临时邮箱
    POST /mailboxes 无需请求体，响应中包含邮箱地址和 Bearer token
    """
    resp = tm_http.post(
        f"{BASE_URL}/mailboxes",
        headers=_HEADERS,
        timeout=15,
    )
    resp.raise_for_status()
    body = resp.json()

    if not isinstance(body, dict):
        raise RuntimeError(f"mailcat-ai: 响应格式无效，响应: {body!r}")

    data = body.get("data")
    if not isinstance(data, dict):
        raise RuntimeError(f"mailcat-ai: 响应 data 格式无效，响应: {body!r}")

    address = data.get("email", "")
    token = data.get("token", "")

    if not address:
        raise RuntimeError(f"mailcat-ai: 缺少邮箱地址，响应: {body!r}")
    if not token:
        raise RuntimeError(f"mailcat-ai: 缺少认证令牌，响应: {body!r}")

    return EmailInfo(
        channel=CHANNEL,
        email=address,
        _token=token,
    )


def get_emails(token: str, email: str) -> List[Email]:
    """
    获取 mailcat.ai 邮件列表
    GET /inbox，需要 Authorization: Bearer {token} 请求头
    """
    if not token:
        raise ValueError("mailcat-ai: token 不能为空")

    address = (email or "").strip()
    if not address:
        raise ValueError("mailcat-ai: 缺少邮箱地址")

    headers = {
        **_HEADERS,
        "Authorization": f"Bearer {token}",
    }

    resp = tm_http.get(
        f"{BASE_URL}/inbox",
        headers=headers,
        timeout=15,
    )
    if resp.status_code == 404:
        return []
    resp.raise_for_status()

    body = resp.json()
    if not isinstance(body, dict):
        return []

    messages = body.get("data")
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
