"""
CleanTempMail 渠道 — https://cleantempmail.com
"""

import os
from typing import Any, Dict, List
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "cleantempmail"
API_BASE = "https://cleantempmail.com/api"
API_KEY = os.environ.get("CLEANTEMPMAIL_API_KEY", "").strip() or "ct-test"
HEADERS = {
    "Accept": "application/json",
    "User-Agent": "Mozilla/5.0",
    "X-API-Key": API_KEY,
}


def _get_json(path: str) -> Dict[str, Any]:
    resp = tm_http.get(f"{API_BASE}{path}", headers=HEADERS, timeout=15)
    resp.raise_for_status()
    data = resp.json()
    return data if isinstance(data, dict) else {}


def generate_email() -> EmailInfo:
    data = _get_json("/generate-email")
    payload = data.get("data") if isinstance(data, dict) else {}
    email = ""
    if isinstance(payload, dict):
        email = str(payload.get("email") or payload.get("mailbox") or "").strip()
    if not email or "@" not in email:
        raise ValueError("cleantempmail: invalid generate-email response")
    return EmailInfo(channel=CHANNEL, email=email)


def get_emails(email: str) -> List[Email]:
    address = (email or "").strip()
    if not address:
        raise ValueError("cleantempmail: empty email")

    data = _get_json(f"/emails?email={quote(address, safe='')}")
    payload = data.get("data") if isinstance(data, dict) else {}
    rows = payload.get("emails") if isinstance(payload, dict) else []
    if not isinstance(rows, list):
        return []

    emails: List[Email] = []
    for item in rows:
        if isinstance(item, dict):
            emails.append(normalize_email(item, address))
    return emails
