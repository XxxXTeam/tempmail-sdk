"""
GeniusGSM 渠道 — https://tempmail.geniusgsm.com

列表: GET /api/domains/ 选择激活域名。
创建: POST /api/inboxes/create_inbox/ {domain_id,ttl} → id/email。
收件: GET /api/inboxes/{id}/emails/（响应已包含 body_text/body_html）。
"""

from typing import List
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "geniusgsm"
BASE_URL = "https://tempmail.geniusgsm.com/api"
_HEADERS = {"Accept": "application/json", "Content-Type": "application/json"}


def _rows(data) -> list:
    if isinstance(data, list):
        return data
    if isinstance(data, dict):
        if isinstance(data.get("results"), list):
            return data["results"]
    return []


def _from_value(raw: dict) -> str:
    name = str(raw.get("from_name", "") or "")
    addr = str(raw.get("from_email", "") or "")
    return f"{name} <{addr}>" if name and addr else addr


def generate_email(**kwargs) -> EmailInfo:
    """选择激活域名并创建收件箱"""
    domains = tm_http.get(f"{BASE_URL}/domains/", headers=_HEADERS, timeout=20)
    domains.raise_for_status()
    active = next(
        (x for x in _rows(domains.json()) if x.get("is_active")),
        None,
    )
    if not active:
        raise ValueError("geniusgsm: 无可用激活域名")

    resp = tm_http.post(
        f"{BASE_URL}/inboxes/create_inbox/",
        headers=_HEADERS,
        json={"domain_id": active["id"], "ttl": 3600},
        timeout=20,
    )
    resp.raise_for_status()
    data = resp.json()
    inbox_id = str(data.get("id", "")).strip()
    address = str(data.get("email", "")).strip()
    if not inbox_id or not address:
        raise ValueError("geniusgsm: 创建响应不完整")
    return EmailInfo(
        channel=CHANNEL,
        email=address,
        _token=inbox_id,
        expires_at=data.get("expires_at"),
        created_at=data.get("created_at"),
    )


def get_emails(email: str, token: str) -> List[Email]:
    """列表响应已包含完整正文"""
    inbox_id = (token or "").strip()
    if not inbox_id:
        raise ValueError("geniusgsm: 收件箱 ID 为空")

    resp = tm_http.get(
        f"{BASE_URL}/inboxes/{quote(inbox_id, safe='')}/emails/",
        headers=_HEADERS,
        timeout=20,
    )
    resp.raise_for_status()
    out: List[Email] = []
    for item in _rows(resp.json()):
        out.append(
            normalize_email(
                {
                    "id": item.get("id"),
                    "from": _from_value(item),
                    "to": item.get("inbox_email") or email,
                    "subject": item.get("subject"),
                    "text": item.get("body_text"),
                    "html": item.get("body_html"),
                    "date": item.get("received_at"),
                    "read": item.get("is_read"),
                    "attachments": item.get("attachments"),
                },
                email,
            )
        )
    return out
