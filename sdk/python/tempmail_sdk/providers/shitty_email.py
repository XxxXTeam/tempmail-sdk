"""
shitty.email 渠道 — https://shitty.email
"""

from typing import Any, Dict, List, Optional
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "shitty-email"
API_BASE = "https://shitty.email/api"
HEADERS = {"Accept": "application/json", "User-Agent": "Mozilla/5.0"}


def _headers(token: Optional[str] = None) -> Dict[str, str]:
    headers = dict(HEADERS)
    if token:
        headers["X-Session-Token"] = token
    return headers


def _message_raw(raw: Dict[str, Any], recipient: str) -> Dict[str, Any]:
    return {
        "id": raw.get("id"),
        "from": raw.get("from"),
        "to": raw.get("to") or recipient,
        "subject": raw.get("subject"),
        "text": raw.get("text"),
        "html": raw.get("html"),
        "date": raw.get("date"),
    }


def _fetch_message(token: str, message_id: str) -> Optional[Dict[str, Any]]:
    resp = tm_http.get(
        f"{API_BASE}/email/{quote(message_id, safe='')}",
        headers=_headers(token),
        timeout=15,
    )
    if not resp.ok:
        return None
    data = resp.json()
    return data if isinstance(data, dict) else None


def generate_email() -> EmailInfo:
    resp = tm_http.post(f"{API_BASE}/inbox", headers=_headers(), timeout=15)
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, dict):
        raise ValueError("shitty-email: invalid inbox response")
    email = str(data.get("email") or "").strip()
    token = str(data.get("token") or "").strip()
    if not email or "@" not in email or not token:
        raise ValueError("shitty-email: invalid inbox response")

    return EmailInfo(
        channel=CHANNEL,
        email=email,
        _token=token,
        expires_at=data.get("expiresAt"),
    )


def get_emails(token: str, email: str) -> List[Email]:
    session_token = (token or "").strip()
    address = (email or "").strip()
    if not session_token:
        raise ValueError("shitty-email: empty token")
    if not address:
        raise ValueError("shitty-email: empty email")

    resp = tm_http.get(f"{API_BASE}/inbox", headers=_headers(session_token), timeout=15)
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, dict):
        return []
    rows = data.get("emails")
    if not isinstance(rows, list):
        return []

    emails: List[Email] = []
    for item in rows:
        if not isinstance(item, dict):
            continue
        message_id = str(item.get("id") or "").strip()
        detail = _fetch_message(session_token, message_id) if message_id else None
        emails.append(normalize_email(_message_raw(detail or item, address), address))
    return emails
