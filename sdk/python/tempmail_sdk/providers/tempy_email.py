"""
Tempy Email 渠道
"""

from typing import Any, Dict, List, Optional
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "tempy-email"
API_BASE = "https://tempy.email/api/v1"
HEADERS = {
    "Accept": "application/json",
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
}


def _flatten_message(raw: Dict[str, Any], recipient: str) -> Dict[str, Any]:
    return {
        "id": raw.get("id")
        or raw.get("messageId")
        or raw.get("message_id")
        or raw.get("mail_id")
        or "",
        "from": raw.get("from")
        or raw.get("sender")
        or raw.get("from_addr")
        or raw.get("from_address")
        or "",
        "to": raw.get("to") or recipient,
        "subject": raw.get("subject") or raw.get("mail_title") or "",
        "text": raw.get("text")
        or raw.get("body_text")
        or raw.get("text_body")
        or raw.get("body")
        or "",
        "html": raw.get("html") or raw.get("body_html") or raw.get("html_body") or "",
        "date": raw.get("date")
        or raw.get("received_at")
        or raw.get("created_at")
        or "",
        "is_read": raw.get("is_read") or raw.get("isRead") or raw.get("seen") or False,
        "attachments": raw.get("attachments") or [],
    }


def generate_email(domain: Optional[str] = None) -> EmailInfo:
    body: Dict[str, Any] = {}
    wanted = (domain or "").strip()
    if wanted:
        body["domain"] = wanted

    resp = tm_http.post(
        f"{API_BASE}/mailbox",
        headers={**HEADERS, "Content-Type": "application/json"},
        json=body,
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, dict):
        raise ValueError("tempy-email: invalid create response")
    email = str(data.get("email") or "").strip()
    if not email:
        raise ValueError("tempy-email: invalid create response")
    return EmailInfo(channel=CHANNEL, email=email, expires_at=data.get("expires_at"))


def get_emails(email: str) -> List[Email]:
    address = (email or "").strip()
    if not address:
        raise ValueError("tempy-email: empty email")

    resp = tm_http.get(
        f"{API_BASE}/mailbox/{quote(address, safe='')}/messages",
        headers=HEADERS,
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    rows = data.get("messages") if isinstance(data, dict) else data
    if not isinstance(rows, list):
        return []

    return [
        normalize_email(_flatten_message(item, address), address)
        for item in rows
        if isinstance(item, dict)
    ]
