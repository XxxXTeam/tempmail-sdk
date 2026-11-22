"""
GrabMail 渠道 — https://grabmail.io

地址即邮箱：首封邮件到达时自动存在，无需创建。
列表: GET /api/v1/mailbox?address={email}
详情: GET /api/v1/message/{id}?mailbox={email}
"""

import random
import string
from typing import List
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "grabmail"
BASE_URL = "https://grabmail.io/api/v1"
DOMAIN = "grabmail.io"

_HEADERS = {"Accept": "application/json"}


def _random_local() -> str:
    return "sdk" + "".join(
        random.choices(string.ascii_lowercase + string.digits, k=16)
    )


def _get_json(path: str) -> dict:
    resp = tm_http.get(f"{BASE_URL}{path}", headers=_HEADERS, timeout=20)
    resp.raise_for_status()
    return resp.json()


def _from_value(raw: dict) -> str:
    name = str(raw.get("from_name", "") or "")
    addr = str(raw.get("from", "") or "")
    return f"{name} <{addr}>" if name else addr


def generate_email(**kwargs) -> EmailInfo:
    """生成 GrabMail 地址（无需请求服务端）"""
    return EmailInfo(channel=CHANNEL, email=f"{_random_local()}@{DOMAIN}")


def get_emails(email: str) -> List[Email]:
    """获取 GrabMail 邮件，逐条补全正文"""
    address = (email or "").strip()
    if not address:
        raise ValueError("grabmail: 邮箱地址为空")

    data = _get_json(f"/mailbox?address={quote(address, safe='@.-_~')}")
    out: List[Email] = []
    for item in data.get("messages", []) or []:
        message_id = str(item.get("id", "")).strip()
        if not message_id:
            continue
        try:
            detail = _get_json(
                f"/message/{quote(message_id, safe='')}"
                f"?mailbox={quote(address, safe='@.-_~')}"
            )
            out.append(
                normalize_email(
                    {
                        "id": detail.get("id") or message_id,
                        "from": _from_value(detail),
                        "to": address,
                        "subject": detail.get("subject"),
                        "text": detail.get("text"),
                        "html": detail.get("html"),
                        "date": detail.get("date"),
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
                    "from": _from_value(item),
                    "to": address,
                    "subject": item.get("subject"),
                    "text": item.get("preview"),
                    "date": item.get("date"),
                    "seen": item.get("seen"),
                },
                address,
            )
        )
    return out
