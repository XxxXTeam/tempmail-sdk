"""
TempMailC 渠道 — https://tempmailc.com
"""

from typing import Any, Dict, List, Optional
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "tempmailc"
API_BASE = "https://tempmailc.com/api/v1"
HEADERS = {"Accept": "application/json"}


def _flatten_list_message(raw: Dict[str, Any], recipient: str) -> Dict[str, Any]:
    return {
        "id": raw.get("id"),
        "from": raw.get("from"),
        "to": recipient,
        "subject": raw.get("subject", ""),
        "timestamp": raw.get("ts"),
        "read": raw.get("read"),
    }


def _fetch_message(email: str, message_id: str) -> Optional[Dict[str, Any]]:
    resp = tm_http.get(
        f"{API_BASE}/message?email={quote(email, safe='')}&msg_id={quote(message_id, safe='')}",
        headers=HEADERS,
        timeout=15,
    )
    if not resp.ok:
        return None
    data = resp.json()
    if not isinstance(data, dict) or data.get("ok") is False:
        return None
    return data


def generate_email() -> EmailInfo:
    resp = tm_http.get(f"{API_BASE}/new", headers=HEADERS, timeout=15)
    resp.raise_for_status()
    data = resp.json()
    email = str(data.get("email") or "").strip() if isinstance(data, dict) else ""
    if not isinstance(data, dict) or not data.get("ok") or not email or "@" not in email:
        raise ValueError("tempmailc: invalid new email response")
    return EmailInfo(channel=CHANNEL, email=email)


def get_emails(email: str) -> List[Email]:
    address = (email or "").strip()
    if not address:
        raise ValueError("tempmailc: empty email")

    resp = tm_http.get(
        f"{API_BASE}/inbox?email={quote(address, safe='')}",
        headers=HEADERS,
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, dict) or not data.get("ok"):
        raise ValueError("tempmailc: inbox response failed")

    rows = data.get("messages")
    if not isinstance(rows, list):
        return []

    emails: List[Email] = []
    for item in rows:
        if not isinstance(item, dict):
            continue
        message_id = str(item.get("id") or "").strip()
        detail = _fetch_message(address, message_id) if message_id else None
        emails.append(normalize_email(detail or _flatten_list_message(item, address), address))
    return emails
