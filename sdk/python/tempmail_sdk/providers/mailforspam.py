"""
MailForSpam 渠道 — https://mailforspam.com
"""

import random
import string
from typing import Any, Dict, List, Optional
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "mailforspam"
API_BASE = "https://api.mailforspam.com/api"
DEFAULT_DOMAIN = "mailforspam.com"
DOMAINS = ["mailforspam.com", "tempmail.io", "disposable.email"]

HEADERS = {
    "Accept": "application/json",
    "Referer": "https://mailforspam.com/",
    "Origin": "https://mailforspam.com",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
}


def _random_local() -> str:
    chars = string.ascii_lowercase + string.digits
    return "sdk" + "".join(random.choice(chars) for _ in range(16))


def _pick_domain(preferred: Optional[str]) -> str:
    p = (preferred or "").strip().lstrip("@").lower()
    if p:
        for domain in DOMAINS:
            if domain.lower() == p:
                return domain
    return DEFAULT_DOMAIN


def _mailbox_emails_url(email: str, page_size: int = 100) -> str:
    return (
        f"{API_BASE}/mailboxes/{quote(email, safe='')}/emails"
        f"?page=1&page_size={page_size}&sort_by=date&sort_order=desc"
    )


def _flatten(raw: Dict[str, Any], recipient: str) -> Dict[str, Any]:
    return {
        "id": raw.get("id", ""),
        "from": raw.get("sender", ""),
        "to": raw.get("receiver") or recipient,
        "subject": raw.get("subject", ""),
        "text": raw.get("body_text", ""),
        "html": raw.get("body_html", ""),
        "date": raw.get("date", ""),
        "isRead": raw.get("readAt") is not None,
        "attachments": raw.get("attachments"),
    }


def _fetch_message(message_id: str, email: str) -> Optional[Dict[str, Any]]:
    resp = tm_http.get(
        f"{API_BASE}/mailboxes/{quote(email, safe='')}/emails/{quote(message_id, safe='')}",
        headers=HEADERS,
        timeout=15,
    )
    if not resp.ok:
        return None
    data = resp.json()
    return data if isinstance(data, dict) else None


def generate_email(domain: Optional[str] = None) -> EmailInfo:
    email = f"{_random_local()}@{_pick_domain(domain)}"
    resp = tm_http.get(_mailbox_emails_url(email, 1), headers=HEADERS, timeout=15)
    resp.raise_for_status()
    return EmailInfo(channel=CHANNEL, email=email)


def get_emails(email: str) -> List[Email]:
    address = (email or "").strip()
    if not address:
        raise ValueError("mailforspam: empty email")

    resp = tm_http.get(_mailbox_emails_url(address, 100), headers=HEADERS, timeout=15)
    resp.raise_for_status()
    data = resp.json()
    rows = data.get("emails") if isinstance(data, dict) else []
    if not isinstance(rows, list):
        return []

    emails: List[Email] = []
    for item in rows:
        if not isinstance(item, dict):
            continue
        message_id = str(item.get("id") or "").strip()
        if not message_id:
            continue
        detail = _fetch_message(message_id, address)
        emails.append(normalize_email(_flatten(detail or item, address), address))
    return emails
