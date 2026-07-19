"""
tempgo-email 渠道 — https://tempgo.email

临时邮箱服务，创建时返回 mailbox_id 作为 token。
  1. 创建: POST https://tempgo.email/api/generate（无 body，不发送 Content-Type）
     响应: {"email":"...","expires_at":"...","expires_in":3600,"mailbox_id":"...","token":"..."}
  2. 读信: GET https://tempgo.email/api/inbox?email={email}&mailbox_id={token}
     响应: {"email":"...","expires_in":...,"messages":[...]}
"""

from typing import List

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "tempgo-email"
BASE_URL = "https://tempgo.email"

_HEADERS = {
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36"
    ),
    "Accept": "application/json",
}


def generate_email() -> EmailInfo:
    """
    创建 tempgo.email 临时邮箱
    POST /api/generate 不发送 Content-Type，响应中 mailbox_id 作为 token
    """
    resp = tm_http.post(
        f"{BASE_URL}/api/generate",
        headers=_HEADERS,
        timeout=15,
    )
    resp.raise_for_status()
    body = resp.json()

    if not isinstance(body, dict):
        raise RuntimeError(f"tempgo-email: 响应格式无效，响应: {body!r}")

    address = body.get("email", "")
    mailbox_id = body.get("mailbox_id", "")

    if not address:
        raise RuntimeError(f"tempgo-email: 缺少邮箱地址，响应: {body!r}")
    if not mailbox_id:
        raise RuntimeError(f"tempgo-email: 缺少 mailbox_id，响应: {body!r}")

    return EmailInfo(
        channel=CHANNEL,
        email=address,
        _token=mailbox_id,
    )


def get_emails(token: str, email: str) -> List[Email]:
    """
    获取 tempgo.email 邮件列表
    GET /api/inbox?email={email}&mailbox_id={token}
    """
    if not token:
        raise ValueError("tempgo-email: token 不能为空")

    address = (email or "").strip()
    if not address:
        raise ValueError("tempgo-email: 缺少邮箱地址")

    resp = tm_http.get(
        f"{BASE_URL}/api/inbox",
        params={"email": address, "mailbox_id": token},
        headers=_HEADERS,
        timeout=15,
    )
    if resp.status_code == 404:
        return []
    resp.raise_for_status()

    body = resp.json()
    if not isinstance(body, dict):
        return []

    messages = body.get("messages")
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
