"""
DummyInbox 渠道 — https://dummyinbox.com

无需密钥；创建随机地址后，地址本身即读取凭据。
创建: POST /api/v1/inboxes
列表: GET /api/v1/inboxes/{address}/messages
详情: GET /api/v1/inboxes/{address}/messages/{id}
"""

from typing import List
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "dummyinbox"
BASE_URL = "https://dummyinbox.com/api/v1"

_HEADERS = {"Accept": "application/json"}


def _path(value: str) -> str:
    """保留地址中的 @，仅转义其他字符"""
    return quote(value.strip(), safe="@.-_~")


def _get_json(path: str) -> dict:
    resp = tm_http.get(f"{BASE_URL}{path}", headers=_HEADERS, timeout=20)
    resp.raise_for_status()
    return resp.json()


def generate_email(**kwargs) -> EmailInfo:
    """创建 DummyInbox 临时邮箱"""
    resp = tm_http.post(f"{BASE_URL}/inboxes", headers=_HEADERS, timeout=20)
    resp.raise_for_status()
    data = resp.json()
    address = str(data.get("address", "")).strip()
    if not address:
        raise ValueError("dummyinbox: 创建响应缺少地址")
    return EmailInfo(channel=CHANNEL, email=address)


def get_emails(email: str) -> List[Email]:
    """获取 DummyInbox 邮件，逐条补全正文"""
    address = (email or "").strip()
    if not address:
        raise ValueError("dummyinbox: 邮箱地址为空")

    path_addr = _path(address)
    data = _get_json(f"/inboxes/{path_addr}/messages?limit=50")
    out: List[Email] = []
    for item in data.get("messages", []) or []:
        message_id = str(item.get("id", "")).strip()
        if not message_id:
            continue
        try:
            detail = _get_json(
                f"/inboxes/{path_addr}/messages/{_path(message_id)}"
            )
            message = detail.get("message")
            if isinstance(message, dict):
                to = message.get("to")
                if isinstance(to, list) and to:
                    to = to[0]
                out.append(
                    normalize_email(
                        {
                            "id": message.get("id"),
                            "from": message.get("from"),
                            "to": to or address,
                            "subject": message.get("subject"),
                            "text": message.get("text"),
                            "html": message.get("html"),
                            "date": message.get("receivedAt"),
                            "attachments": message.get("attachments"),
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
                    "from": item.get("from"),
                    "to": address,
                    "subject": item.get("subject"),
                    "text": item.get("snippet"),
                    "date": item.get("receivedAt"),
                },
                address,
            )
        )
    return out
