"""
TempMail Pro 渠道 - https://tempmailpro.us
"""

from typing import Any, Dict, List, Optional
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "tempmailpro"
API_BASE = "https://tempmailpro.us/api/v1"
HEADERS = {"Accept": "application/json", "User-Agent": "Mozilla/5.0"}


def _flatten(raw: Dict[str, Any], recipient: str) -> Dict[str, Any]:
    return {
        "id": raw.get("id") or raw.get("message_id"),
        "from": raw.get("from_address") or raw.get("from_name"),
        "to": recipient,
        "subject": raw.get("subject"),
        "text": raw.get("body_text"),
        "html": raw.get("body_html"),
        "date": raw.get("received_at"),
        "attachments": raw.get("attachments"),
    }


def generate_email() -> EmailInfo:
    resp = tm_http.post(
        f"{API_BASE}/mailbox/create",
        headers={**HEADERS, "Content-Type": "application/json"},
        json={},
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, dict):
        raise ValueError("tempmailpro: invalid mailbox response")
    box = data.get("data") or {}
    if not isinstance(box, dict):
        raise ValueError("tempmailpro: invalid mailbox response")
    email = str(box.get("address") or "").strip()
    token = str(box.get("token") or "").strip()
    if not email or "@" not in email or not token:
        raise ValueError("tempmailpro: invalid mailbox response")

    return EmailInfo(
        channel=CHANNEL,
        email=email,
        _token=token,
        expires_at=box.get("expires_at"),
        created_at=box.get("created_at"),
    )


def _fetch_detail(token: str, message_id: str) -> Optional[Dict[str, Any]]:
    resp = tm_http.get(
        f"{API_BASE}/mailbox/{quote(token, safe='')}/emails/{quote(message_id, safe='')}",
        headers=HEADERS,
        timeout=15,
    )
    if not resp.ok:
        return None
    data = resp.json()
    if not isinstance(data, dict):
        return None
    detail = data.get("data")
    return detail if isinstance(detail, dict) else None


def get_emails(token: str, email: str) -> List[Email]:
    mailbox_token = (token or "").strip()
    address = (email or "").strip()
    if not mailbox_token:
        raise ValueError("tempmailpro: empty token")
    if not address:
        raise ValueError("tempmailpro: empty email")

    resp = tm_http.get(
        f"{API_BASE}/mailbox/{quote(mailbox_token, safe='')}/emails",
        headers=HEADERS,
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, dict):
        return []
    rows = data.get("data")
    if not isinstance(rows, list):
        return []

    emails: List[Email] = []
    for item in rows:
        if not isinstance(item, dict):
            continue
        message_id = str(item.get("id") or "").strip()
        detail = _fetch_detail(mailbox_token, message_id) if message_id else None
        emails.append(normalize_email(_flatten(detail or item, address), address))
    return emails
