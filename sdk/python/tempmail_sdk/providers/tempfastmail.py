"""
TempFastMail 渠道 -- https://tempfastmail.com
"""

from __future__ import annotations

from typing import Any, Dict, List
from urllib.parse import quote

from ..normalize import normalize_email
from ..types import Email, EmailInfo
from .. import http

CHANNEL = "tempfastmail"
BASE_URL = "https://tempfastmail.com"
HEADERS = {"Accept": "application/json", "User-Agent": "Mozilla/5.0"}


def _flatten(raw: Dict[str, Any], recipient: str) -> Dict[str, Any]:
    return {
        "id": raw.get("uuid"),
        "from": raw.get("from"),
        "to": raw.get("real_to") or recipient,
        "subject": raw.get("subject"),
        "html": raw.get("html"),
        "date": raw.get("received_at"),
        "attachments": raw.get("attachments"),
    }


def generate_email() -> EmailInfo:
    response = http.post(f"{BASE_URL}/api/email-box", headers=HEADERS)
    response.raise_for_status()
    data = response.json()
    email = str(data.get("email") or "").strip()
    uuid = str(data.get("uuid") or "").strip()
    if not email or "@" not in email or not uuid:
        raise ValueError("tempfastmail: invalid create response")
    return EmailInfo(channel=CHANNEL, email=email, token=uuid)


def get_emails(token: str, email: str) -> List[Email]:
    uuid = str(token or "").strip()
    address = str(email or "").strip()
    if not uuid:
        raise ValueError("tempfastmail: empty token")
    if not address:
        raise ValueError("tempfastmail: empty email")

    response = http.get(
        f"{BASE_URL}/api/email-box/{quote(uuid, safe='')}/emails", headers=HEADERS
    )
    response.raise_for_status()
    rows = response.json()
    if not isinstance(rows, list):
        return []

    out: List[Email] = []
    for row in rows:
        raw = row if isinstance(row, dict) else {}
        message_id = str(raw.get("uuid") or "").strip()
        if message_id:
            detail_response = http.get(
                f"{BASE_URL}/api/email-box/{quote(uuid, safe='')}/email/{quote(message_id, safe='')}",
                headers=HEADERS,
            )
            if detail_response.ok:
                detail = detail_response.json()
                if isinstance(detail, dict):
                    raw = detail
        out.append(normalize_email(_flatten(raw, address), address))
    return out
