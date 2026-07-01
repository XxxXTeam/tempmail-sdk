"""
Mail10s 渠道 -- https://mail10s.com
"""

import random
import string
from typing import Any, Dict, List
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "mail10s"
BASE_URL = "https://mail10s.com"
DOMAIN = "mail10s.com"
HEADERS = {"Accept": "application/json", "User-Agent": "Mozilla/5.0"}


def _random_local() -> str:
    chars = string.ascii_lowercase + string.digits
    return "sdk" + "".join(random.choice(chars) for _ in range(16))


def _flatten(raw: Dict[str, Any], recipient: str) -> Dict[str, Any]:
    return {
        "id": raw.get("id"),
        "from": raw.get("sender"),
        "to": recipient,
        "subject": raw.get("subject"),
        "text": raw.get("body_text"),
        "html": raw.get("body_html"),
        "date": raw.get("received_at"),
        "attachments": raw.get("attachments"),
    }


def generate_email() -> EmailInfo:
    email = f"{_random_local()}@{DOMAIN}"
    return EmailInfo(channel=CHANNEL, email=email, _token=email)


def get_emails(email: str) -> List[Email]:
    address = (email or "").strip()
    if not address:
        raise ValueError("mail10s: empty email")
    resp = tm_http.get(
        f"{BASE_URL}/api/emails/{quote(address, safe='')}/inbox",
        headers=HEADERS,
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    messages = data.get("data", {}).get("messages") if isinstance(data, dict) else []
    if not isinstance(messages, list):
        return []
    return [normalize_email(_flatten(row, address), address) for row in messages if isinstance(row, dict)]
