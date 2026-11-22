"""
MailboxTemp 渠道 — https://mailboxtemp.com

无需密钥；创建免费邮箱后，地址即读取凭据。
创建: POST /api/inbox/generate
列表: GET /api/inbox/{address}/emails
详情: GET /api/email/{address}/{id}
"""

from typing import List
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "mailboxtemp"
BASE_URL = "https://mailboxtemp.com"

_HEADERS = {"Accept": "application/json"}


def _get_json(path: str) -> dict:
    resp = tm_http.get(f"{BASE_URL}{path}", headers=_HEADERS, timeout=20)
    data = resp.json()
    if resp.status_code >= 400 or data.get("ok") is False:
        raise RuntimeError(f"mailboxtemp: HTTP {resp.status_code}")
    return data


def generate_email(**kwargs) -> EmailInfo:
    """创建 MailboxTemp 临时邮箱"""
    resp = tm_http.post(f"{BASE_URL}/api/inbox/generate", headers=_HEADERS, timeout=20)
    data = resp.json()
    address = str(data.get("address", "")).strip()
    if resp.status_code >= 400 or data.get("ok") is False or not address:
        raise RuntimeError(f"mailboxtemp: 创建邮箱 HTTP {resp.status_code}")
    return EmailInfo(
        channel=CHANNEL,
        email=address,
        expires_at=data.get("expires"),
    )


def get_emails(email: str) -> List[Email]:
    """获取 MailboxTemp 邮件，逐条补全正文"""
    address = (email or "").strip()
    if not address:
        raise ValueError("mailboxtemp: 邮箱地址为空")

    path_addr = quote(address, safe="@.-_~")
    data = _get_json(f"/api/inbox/{path_addr}/emails")
    out: List[Email] = []
    for item in data.get("emails", []) or []:
        message_id = str(item.get("id", "")).strip()
        if not message_id:
            continue
        try:
            detail = _get_json(f"/api/email/{path_addr}/{quote(message_id, safe='')}")
            message = detail.get("email")
            if isinstance(message, dict):
                out.append(
                    normalize_email(
                        {
                            "id": message.get("id"),
                            "from": message.get("from_address"),
                            "to": address,
                            "subject": message.get("subject"),
                            "text": message.get("body_text"),
                            "html": message.get("body_html"),
                            "date": message.get("received_at"),
                            "read": message.get("read"),
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
                    "from": item.get("from_address"),
                    "to": address,
                    "subject": item.get("subject"),
                    "date": item.get("received_at"),
                    "read": item.get("read"),
                },
                address,
            )
        )
    return out
