"""
DevMail UK 渠道 — https://devmail.uk
"""

from typing import Any, Dict, List
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "devmail-uk"
API_BASE = "https://www.devmail.uk/api"
HEADERS = {"Accept": "application/json"}


def _mailbox_from_email(email: str) -> str:
    address = (email or "").strip()
    if not address:
        return ""
    if "@" in address:
        return address.split("@", 1)[0].strip()
    return address


def generate_email() -> EmailInfo:
    resp = tm_http.get(f"{API_BASE}/new", headers=HEADERS, timeout=15)
    resp.raise_for_status()
    data = resp.json()
    email = str(data.get("email") or "").strip() if isinstance(data, dict) else ""
    if not email and isinstance(data, dict) and str(data.get("mailbox") or "").strip():
        email = f"{str(data.get('mailbox')).strip()}@devmail.uk"
    if not isinstance(data, dict) or not email or "@" not in email:
        raise ValueError("devmail-uk: invalid new email response")
    return EmailInfo(channel=CHANNEL, email=email)


def get_emails(email: str) -> List[Email]:
    mailbox = _mailbox_from_email(email)
    if not mailbox:
        raise ValueError("devmail-uk: empty email")

    resp = tm_http.get(
        f"{API_BASE}/inbox/{quote(mailbox, safe='')}?detail=true",
        headers=HEADERS,
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, dict):
        return []

    rows = data.get("emails")
    if not isinstance(rows, list):
        return []

    emails: List[Email] = []
    for item in rows:
        if isinstance(item, dict):
            emails.append(normalize_email(item, email))
    return emails
