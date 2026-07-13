"""
ThrowawayMail 渠道 — https://throwawaymail.app
"""

from typing import Any, Dict, List, Optional
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "throwawaymail"
API_BASE = "https://throwawaymail.app/api"
HEADERS = {"Accept": "application/json"}


def _message_raw(raw: Dict[str, Any], recipient: str) -> Dict[str, Any]:
    to_addresses = raw.get("to_addresses")
    return {
        "id": raw.get("message_id"),
        "messageId": raw.get("message_id"),
        "from_address": raw.get("from_address"),
        "fromName": raw.get("from_name"),
        "to": (
            to_addresses[0]
            if isinstance(to_addresses, list) and to_addresses
            else recipient
        ),
        "subject": raw.get("subject", ""),
        "received_at": raw.get("received_at"),
        "read": raw.get("read"),
        "text": raw.get("text") or "",
        "html": raw.get("html") or "",
        "size": raw.get("size"),
    }


def _fetch_message(mailbox_id: str, message_id: str) -> Optional[Dict[str, Any]]:
    resp = tm_http.get(
        f"{API_BASE}/mailboxes/{quote(mailbox_id, safe='')}/messages/{quote(message_id, safe='')}",
        headers=HEADERS,
        timeout=15,
    )
    if not resp.ok:
        return None
    data = resp.json()
    return data if isinstance(data, dict) else None


def generate_email() -> EmailInfo:
    resp = tm_http.post(f"{API_BASE}/mailboxes", headers=HEADERS, timeout=15)
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, dict):
        raise ValueError("throwawaymail: invalid mailbox response")
    mailbox_id = str(data.get("mailbox_id") or "").strip()
    email = str(data.get("address") or "").strip()
    if not mailbox_id or not email or "@" not in email:
        raise ValueError("throwawaymail: invalid mailbox response")

    return EmailInfo(
        channel=CHANNEL,
        email=email,
        _token=mailbox_id,
        expires_at=data.get("expires_at"),
        created_at=data.get("created_at"),
    )


def get_emails(token: str, email: str) -> List[Email]:
    mailbox_id = (token or "").strip()
    address = (email or "").strip()
    if not mailbox_id:
        raise ValueError("throwawaymail: empty mailbox id")
    if not address:
        raise ValueError("throwawaymail: empty email")

    resp = tm_http.get(
        f"{API_BASE}/mailboxes/{quote(mailbox_id, safe='')}/messages",
        headers=HEADERS,
        timeout=15,
    )
    resp.raise_for_status()
    rows = resp.json()
    if not isinstance(rows, list):
        return []

    emails: List[Email] = []
    for item in rows:
        if not isinstance(item, dict):
            continue
        message_id = str(item.get("message_id") or "").strip()
        detail = _fetch_message(mailbox_id, message_id) if message_id else None
        emails.append(normalize_email(_message_raw(detail or item, address), address))
    return emails
