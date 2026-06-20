"""
UnCorreoTemporal 渠道 -- https://uncorreotemporal.com
"""

from typing import Any, Dict, List, Optional
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "uncorreotemporal"
API_BASE = "https://uncorreotemporal.com/api/v1"
HEADERS = {
    "Accept": "application/json",
    "Origin": "https://uncorreotemporal.com",
    "Referer": "https://uncorreotemporal.com/",
    "User-Agent": "Mozilla/5.0",
}


def _flatten(raw: Dict[str, Any], recipient: str) -> Dict[str, Any]:
    return {
        "id": raw.get("id") or "",
        "from": raw.get("from_address") or "",
        "to": raw.get("to_address") or recipient,
        "subject": raw.get("subject") or "",
        "text": raw.get("body_text") or "",
        "html": raw.get("body_html") or "",
        "date": raw.get("received_at") or "",
        "isRead": bool(raw.get("is_read")),
        "attachments": raw.get("attachments") if isinstance(raw.get("attachments"), list) else [],
    }


def generate_email() -> EmailInfo:
    resp = tm_http.post(
        f"{API_BASE}/mailboxes",
        headers={**HEADERS, "Content-Type": "application/json"},
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, dict):
        raise ValueError("uncorreotemporal: invalid mailbox response")
    email = str(data.get("address") or "").strip()
    token = str(data.get("session_token") or "").strip()
    if not email or "@" not in email or not token:
        raise ValueError("uncorreotemporal: invalid mailbox response")
    return EmailInfo(
        channel=CHANNEL,
        email=email,
        _token=token,
        expires_at=data.get("expires_at"),
    )


def _fetch_detail(email: str, token: str, message_id: str) -> Optional[Dict[str, Any]]:
    try:
        resp = tm_http.get(
            f"{API_BASE}/mailboxes/{quote(email, safe='')}/messages/{quote(message_id, safe='')}",
            headers={**HEADERS, "X-Session-Token": token},
            timeout=15,
        )
        if not resp.ok:
            return None
        data = resp.json()
        return data if isinstance(data, dict) else None
    except Exception:
        return None


def get_emails(token: str, email: str) -> List[Email]:
    session_token = (token or "").strip()
    address = (email or "").strip()
    if not session_token:
        raise ValueError("uncorreotemporal: empty session token")
    if not address:
        raise ValueError("uncorreotemporal: empty email")

    resp = tm_http.get(
        f"{API_BASE}/mailboxes/{quote(address, safe='')}/messages?limit=50",
        headers={**HEADERS, "X-Session-Token": session_token},
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    rows = data if isinstance(data, list) else []

    emails: List[Email] = []
    for row in rows:
        if not isinstance(row, dict):
            continue
        message_id = str(row.get("id") or "").strip()
        detail = _fetch_detail(address, session_token, message_id) if message_id else None
        emails.append(normalize_email(_flatten(detail or row, address), address))
    return emails
