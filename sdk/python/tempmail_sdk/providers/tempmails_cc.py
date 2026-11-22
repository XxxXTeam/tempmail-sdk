"""
tempmails.cc 渠道 — https://tempmails.cc

地址即邮箱：在公开免费域名上生成地址，无需注册。
列表: GET /api/inbox/{email}
详情: GET /api/message/{id}?address={email}&mark_read=false
详情正文位于 textBody / htmlBody。
"""

import random
import string
from typing import List
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "tempmails-cc"
BASE_URL = "https://tempmails.cc"

_HEADERS = {"Accept": "application/json"}


def _random_local() -> str:
    return "".join(random.choices(string.ascii_lowercase + string.digits, k=16))


def _get_json(path: str) -> dict:
    resp = tm_http.get(f"{BASE_URL}{path}", headers=_HEADERS, timeout=20)
    resp.raise_for_status()
    return resp.json()


def _pick_domain() -> str:
    data = _get_json("/api/domains")
    for item in data.get("domainInfo", []) or []:
        domain = str(item.get("domain", "") or "").strip()
        if domain:
            return domain
    raise ValueError("tempmails-cc: 无可用公开域名")


def generate_email(**kwargs) -> EmailInfo:
    """在公开免费域名上生成地址"""
    domain = _pick_domain()
    return EmailInfo(channel=CHANNEL, email=f"{_random_local()}@{domain}")


def generate_email_at_domain(domain: str, channel: str = CHANNEL) -> EmailInfo:
    """跳过选域，在指定固定域名上生成地址"""
    domain = (domain or "").strip()
    if not domain:
        raise ValueError("tempmails-cc: 域名为空")
    return EmailInfo(channel=channel, email=f"{_random_local()}@{domain}")


def get_emails(email: str) -> List[Email]:
    """获取 tempmails.cc 邮件，详情正文位于 textBody / htmlBody"""
    address = (email or "").strip()
    if not address:
        raise ValueError("tempmails-cc: 邮箱地址为空")

    data = _get_json(f"/api/inbox/{quote(address, safe='@.-_~')}")
    out: List[Email] = []
    for item in data.get("messages", []) or []:
        message_id = str(item.get("id", "")).strip()
        if not message_id:
            continue
        try:
            detail = _get_json(
                f"/api/message/{quote(message_id, safe='')}"
                f"?address={quote(address, safe='@.-_~')}&mark_read=false"
            )
            to = detail.get("to")
            if isinstance(to, list):
                to = to[0] if to else address
            out.append(
                normalize_email(
                    {
                        "id": detail.get("id") or message_id,
                        "from": detail.get("from"),
                        "to": to or address,
                        "subject": detail.get("subject"),
                        "text": detail.get("textBody"),
                        "html": detail.get("htmlBody"),
                        "date": detail.get("date"),
                        "seen": detail.get("isRead"),
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
                    "date": item.get("date"),
                    "seen": item.get("isRead"),
                },
                address,
            )
        )
    return out
