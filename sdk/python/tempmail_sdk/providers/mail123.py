"""
Mail123 渠道 -- https://mail123.fr
"""

import time
from typing import Any, Dict, List, Optional
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "mail123"
API_BASE = "https://mail123.fr/api/v1"
HEADERS = {"Accept": "application/json", "User-Agent": "Mozilla/5.0"}


def _flatten(raw: Dict[str, Any], recipient: str) -> Dict[str, Any]:
    out = dict(raw)
    out["id"] = raw.get("id")
    out["to"] = raw.get("to") or recipient
    out["text"] = raw.get("text") or raw.get("preview") or ""
    out["html"] = raw.get("html") or ""
    if isinstance(raw.get("is_unread"), bool):
        out["isRead"] = not raw["is_unread"]
    return out


def generate_email() -> EmailInfo:
    resp = tm_http.get(f"{API_BASE}/mailbox/new", headers=HEADERS, timeout=15)
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, dict):
        raise ValueError("mail123: invalid mailbox response")
    email = str(data.get("address") or "").strip()
    if not email or "@" not in email:
        raise ValueError("mail123: invalid mailbox response")
    expires_at = None
    if (
        isinstance(data.get("expires_in_days"), (int, float))
        and data["expires_in_days"] > 0
    ):
        expires_at = int((time.time() + float(data["expires_in_days"]) * 86400) * 1000)
    return EmailInfo(channel=CHANNEL, email=email, _token=email, expires_at=expires_at)


def _fetch_detail(address: str, message_id: str) -> Optional[Dict[str, Any]]:
    try:
        resp = tm_http.get(
            f"{API_BASE}/mailbox/{quote(address, safe='')}/messages/{quote(message_id, safe='')}",
            headers=HEADERS,
            timeout=15,
        )
        if not resp.ok:
            return None
        data = resp.json()
        if isinstance(data, dict) and isinstance(data.get("message"), dict):
            return data["message"]
    except Exception:
        return None
    return None


def get_emails(email: str) -> List[Email]:
    address = (email or "").strip()
    if not address:
        raise ValueError("mail123: empty email")
    resp = tm_http.get(
        f"{API_BASE}/mailbox/{quote(address, safe='')}/messages?limit=50",
        headers=HEADERS,
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    rows = data.get("messages") if isinstance(data, dict) else []
    if not isinstance(rows, list):
        return []
    emails: List[Email] = []
    for item in rows:
        if not isinstance(item, dict):
            continue
        message_id = str(item.get("id") or "").strip()
        detail = _fetch_detail(address, message_id) if message_id else None
        emails.append(normalize_email(_flatten(detail or item, address), address))
    return emails
