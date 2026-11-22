"""
Tempmailify 渠道 — https://tempmailify.com

创建: POST /api/inbox（返回 address、token）
列表/详情需 Bearer 令牌；详情提供完整 text/html。
"""

from typing import List
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "tempmailify"
BASE_URL = "https://tempmailify.com"


def _from_address(value) -> str:
    if isinstance(value, dict):
        return str(value.get("address", "") or "")
    return str(value or "")


def _get_json(path: str, bearer: str) -> dict:
    resp = tm_http.get(
        f"{BASE_URL}{path}",
        headers={"Accept": "application/json", "Authorization": f"Bearer {bearer}"},
        timeout=20,
    )
    resp.raise_for_status()
    return resp.json()


def generate_email(**kwargs) -> EmailInfo:
    """创建 Tempmailify 临时邮箱"""
    resp = tm_http.post(
        f"{BASE_URL}/api/inbox",
        headers={"Accept": "application/json", "Content-Type": "application/json"},
        json={},
        timeout=20,
    )
    resp.raise_for_status()
    data = resp.json()
    address = str(data.get("address", "")).strip()
    token = str(data.get("token", "")).strip()
    if not address or not token:
        raise ValueError("tempmailify: 创建响应不完整")
    return EmailInfo(
        channel=CHANNEL,
        email=address,
        _token=token,
        expires_at=data.get("expiresAt"),
        created_at=data.get("createdAt"),
    )


def get_emails(email: str, token: str) -> List[Email]:
    """获取 Tempmailify 邮件，详情提供完整 text/html"""
    address = (email or "").strip()
    if not address or not token:
        raise ValueError("tempmailify: 邮箱地址或令牌为空")

    base = f"/api/inbox/{quote(address, safe='@.-_~')}"
    data = _get_json(base, token)
    out: List[Email] = []
    for item in data.get("messages", []) or []:
        message_id = str(item.get("id", "")).strip()
        if not message_id:
            continue
        try:
            detail = _get_json(f"{base}/messages/{quote(message_id, safe='')}", token)
            out.append(
                normalize_email(
                    {
                        "id": detail.get("id") or message_id,
                        "from": _from_address(detail.get("from")),
                        "to": detail.get("to") or address,
                        "subject": detail.get("subject") or item.get("subject"),
                        "text": detail.get("text"),
                        "html": detail.get("html"),
                        "date": detail.get("receivedAt") or item.get("receivedAt"),
                        "seen": detail.get("seen"),
                        "attachments": detail.get("attachments"),
                    },
                    address,
                )
            )
            continue
        except Exception:
            pass
        out.append(
            normalize_email(
                {
                    "id": message_id,
                    "from": _from_address(item.get("from")),
                    "to": address,
                    "subject": item.get("subject"),
                    "text": item.get("preview"),
                    "date": item.get("receivedAt"),
                    "seen": item.get("seen"),
                },
                address,
            )
        )
    return out
