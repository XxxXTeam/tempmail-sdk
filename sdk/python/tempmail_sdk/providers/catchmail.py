"""
Catchmail 渠道 — https://catchmail.io
"""

import random
import re
import string
from typing import Any, Dict, List, Optional
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "catchmail"
API_BASE = "https://api.catchmail.io/api/v1"
DEFAULT_DOMAIN = "catchmail.io"
DOMAINS = ["catchmail.io", "mailistry.com", "zeppost.com"]

HEADERS = {
    "Accept": "application/json",
    "Referer": "https://catchmail.io/",
    "Origin": "https://catchmail.io",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
}


def _random_local() -> str:
    chars = string.ascii_lowercase + string.digits
    return "sdk" + "".join(random.choice(chars) for _ in range(16))


def _pick_domain(preferred: Optional[str]) -> str:
    p = (preferred or "").strip().lstrip("@").lower()
    if p:
        for domain in DOMAINS:
            if domain.lower() == p:
                return domain
    return DEFAULT_DOMAIN


def _clean_address(value: Any) -> str:
    if isinstance(value, list):
        return _clean_address(value[0]) if value else ""
    text = str(value or "").strip()
    match = re.search(r"<([^>]+)>", text)
    return match.group(1).strip() if match else text


def _flatten_detail(raw: Dict[str, Any], recipient: str) -> Dict[str, Any]:
    body = raw.get("body")
    if not isinstance(body, dict):
        body = {}
    return {
        "id": raw.get("id", ""),
        "from": _clean_address(raw.get("from")),
        "to": _clean_address(raw.get("to")) or recipient,
        "subject": raw.get("subject", ""),
        "text": body.get("text", ""),
        "html": body.get("html", ""),
        "date": raw.get("date", ""),
        "attachments": raw.get("attachments"),
    }


def _fetch_message(message_id: str, email: str) -> Optional[Dict[str, Any]]:
    resp = tm_http.get(
        f"{API_BASE}/message/{quote(message_id, safe='')}?mailbox={quote(email, safe='')}",
        headers=HEADERS,
        timeout=15,
    )
    if not resp.ok:
        return None
    data = resp.json()
    return data if isinstance(data, dict) else None


def generate_email(domain: Optional[str] = None) -> EmailInfo:
    email = f"{_random_local()}@{_pick_domain(domain)}"
    resp = tm_http.get(
        f"{API_BASE}/mailbox?address={quote(email, safe='')}",
        headers=HEADERS,
        timeout=15,
    )
    resp.raise_for_status()
    return EmailInfo(channel=CHANNEL, email=email)


def get_emails(email: str) -> List[Email]:
    address = (email or "").strip()
    if not address:
        raise ValueError("catchmail: empty email")

    resp = tm_http.get(
        f"{API_BASE}/mailbox?address={quote(address, safe='')}",
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
        if not message_id:
            continue
        detail = _fetch_message(message_id, address)
        if detail:
            emails.append(normalize_email(_flatten_detail(detail, address), address))
            continue
        emails.append(normalize_email({
            "id": message_id,
            "from": _clean_address(item.get("from")),
            "to": item.get("mailbox") or address,
            "subject": item.get("subject", ""),
            "date": item.get("date", ""),
        }, address))
    return emails
