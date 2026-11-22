"""
Tsukaisute Mail 渠道 — https://tsukaisutemail.com

无需密钥；创建随机地址后，地址即读取凭据。
创建: POST /api/v1/address
列表: GET /api/v1/inbox/{address}
详情: GET /api/v1/message/{id}
"""

from typing import List
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "tsukaisutemail"
BASE_URL = "https://tsukaisutemail.com/api/v1"

_HEADERS = {"Accept": "application/json"}


def _get_json(path: str) -> dict:
    resp = tm_http.get(f"{BASE_URL}{path}", headers=_HEADERS, timeout=20)
    resp.raise_for_status()
    return resp.json()


def generate_email(**kwargs) -> EmailInfo:
    """创建 Tsukaisute Mail 临时地址"""
    resp = tm_http.post(f"{BASE_URL}/address", headers=_HEADERS, timeout=20)
    resp.raise_for_status()
    address = str(resp.json().get("address", "")).strip()
    if not address:
        raise ValueError("tsukaisutemail: 创建响应缺少地址")
    return EmailInfo(channel=CHANNEL, email=address)


def get_emails(email: str) -> List[Email]:
    """获取 Tsukaisute Mail 邮件，逐条补全正文"""
    address = (email or "").strip()
    if not address:
        raise ValueError("tsukaisutemail: 邮箱地址为空")

    data = _get_json(f"/inbox/{quote(address, safe='@.-_~')}")
    out: List[Email] = []
    for item in data.get("messages", []) or []:
        message_id = str(item.get("id", "")).strip()
        if not message_id:
            continue
        try:
            detail = _get_json(f"/message/{quote(message_id, safe='')}")
            message = detail.get("message")
            if isinstance(message, dict):
                out.append(
                    normalize_email(
                        {
                            "id": message.get("id"),
                            "from": message.get("from_address"),
                            "to": address,
                            "subject": message.get("subject"),
                            "text": message.get("text_body"),
                            "html": message.get("html_body"),
                            "date": message.get("received_at"),
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
                    "from": item.get("from_address"),
                    "to": address,
                    "subject": item.get("subject"),
                    "text": item.get("snippet"),
                    "date": item.get("received_at"),
                },
                address,
            )
        )
    return out
