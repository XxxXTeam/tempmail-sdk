"""
OpenInbox 渠道 -- https://openinbox.io
"""

from typing import Any, Dict, List, Optional
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "openinbox"
API_BASE = "https://api.openinbox.io/api"
HEADERS = {
    "Accept": "application/json, text/plain, */*",
    "Origin": "https://openinbox.io",
    "Referer": "https://openinbox.io/",
    "User-Agent": "Mozilla/5.0",
}


def _flatten(raw: Dict[str, Any], recipient: str) -> Dict[str, Any]:
    return {
        "id": raw.get("id") or "",
        "from": raw.get("from") or "",
        "to": recipient,
        "subject": raw.get("subject") or "",
        "text": raw.get("textBody") or "",
        "html": raw.get("htmlBody") or "",
        "receivedAt": raw.get("receivedAt") or "",
        "isRead": bool(raw.get("isRead")),
        "attachments": [],
    }


def generate_email() -> EmailInfo:
    resp = tm_http.post(
        f"{API_BASE}/inbox",
        headers={**HEADERS, "Content-Type": "application/json"},
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, dict):
        raise ValueError("openinbox: invalid mailbox response")
    inbox_id = str(data.get("id") or "").strip()
    email = str(data.get("email") or "").strip()
    if not inbox_id or not email or "@" not in email:
        raise ValueError("openinbox: invalid mailbox response")
    return EmailInfo(
        channel=CHANNEL,
        email=email,
        _token=inbox_id,
        expires_at=data.get("expiresAt"),
        created_at=data.get("createdAt"),
    )


def _fetch_detail(message_id: str) -> Optional[Dict[str, Any]]:
    try:
        resp = tm_http.get(f"{API_BASE}/emails/{quote(message_id, safe='')}", headers=HEADERS, timeout=15)
        if not resp.ok:
            return None
        data = resp.json()
        return data if isinstance(data, dict) else None
    except Exception:
        return None


def get_emails(token: str, email: str) -> List[Email]:
    inbox_id = (token or "").strip()
    address = (email or "").strip()
    if not inbox_id:
        raise ValueError("openinbox: empty inbox id")
    if not address:
        raise ValueError("openinbox: empty email")

    resp = tm_http.get(
        f"{API_BASE}/emails/inbox/{quote(inbox_id, safe='')}?page=1&limit=50",
        headers=HEADERS,
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    rows = data.get("emails") if isinstance(data, dict) else []
    if not isinstance(rows, list):
        return []

    emails: List[Email] = []
    for row in rows:
        if not isinstance(row, dict):
            continue
        message_id = str(row.get("id") or "").strip()
        detail = _fetch_detail(message_id) if message_id else None
        emails.append(normalize_email(_flatten(detail or row, address), address))
    return emails
