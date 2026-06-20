"""
InboxKitten 渠道 — https://inboxkitten.com
"""

import random
import re
from html import unescape
from typing import Any, Dict, List
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "inboxkitten"
API_BASE = "https://inboxkitten.com/api/v1/mail"
DOMAIN = "inboxkitten.com"
HEADERS_JSON = {"Accept": "application/json"}
HEADERS_HTML = {"Accept": "text/html,*/*"}


def _random_local() -> str:
    chars = "abcdefghijklmnopqrstuvwxyz0123456789"
    return "sdk" + "".join(random.choice(chars) for _ in range(16))


def _html_to_text(html: str) -> str:
    cleaned = re.sub(r"(?is)<script[\s\S]*?</script>", " ", html)
    cleaned = re.sub(r"(?s)<[^>]+>", " ", cleaned)
    return " ".join(unescape(cleaned).split())


def _local_part(email: str) -> str:
    return (email or "").strip().split("@")[0]


def _fetch_meta(region: str, key: str) -> Dict[str, Any]:
    resp = tm_http.get(
        f"{API_BASE}/getKey?region={quote(region, safe='')}&key={quote(key, safe='')}",
        headers=HEADERS_JSON,
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    return data if isinstance(data, dict) else {}


def _fetch_html(region: str, key: str) -> str:
    resp = tm_http.get(
        f"{API_BASE}/getHtml?region={quote(region, safe='')}&key={quote(key, safe='')}",
        headers=HEADERS_HTML,
        timeout=15,
    )
    resp.raise_for_status()
    return resp.text


def _detail_raw(row: Dict[str, Any], recipient: str) -> Dict[str, Any]:
    storage = row.get("storage") if isinstance(row.get("storage"), dict) else {}
    message = row.get("message") if isinstance(row.get("message"), dict) else {}
    headers = message.get("headers") if isinstance(message.get("headers"), dict) else {}
    envelope = row.get("envelope") if isinstance(row.get("envelope"), dict) else {}
    region = str(storage.get("region") or "").strip()
    key = str(storage.get("key") or "").strip()
    raw = {
        "id": key,
        "messageId": key,
        "from": headers.get("from") or envelope.get("sender") or "",
        "to": row.get("recipient") or envelope.get("targets") or recipient,
        "subject": headers.get("subject") or "",
        "timestamp": row.get("timestamp"),
    }
    if not region or not key:
        return raw

    try:
        meta = _fetch_meta(region, key)
        html = _fetch_html(region, key)
        raw.update({
            "from": meta.get("name") or raw["from"],
            "to": meta.get("recipients") or raw["to"],
            "subject": meta.get("subject") or raw["subject"],
            "text": _html_to_text(html),
            "html": html,
        })
    except Exception:
        pass
    return raw


def generate_email() -> EmailInfo:
    return EmailInfo(channel=CHANNEL, email=f"{_random_local()}@{DOMAIN}")


def get_emails(email: str) -> List[Email]:
    local = _local_part(email)
    if not local:
        raise ValueError("inboxkitten: empty email")

    resp = tm_http.get(
        f"{API_BASE}/list?recipient={quote(local, safe='')}",
        headers=HEADERS_JSON,
        timeout=15,
    )
    resp.raise_for_status()
    rows = resp.json()
    if not isinstance(rows, list):
        return []

    emails: List[Email] = []
    for item in rows:
        if isinstance(item, dict):
            emails.append(normalize_email(_detail_raw(item, email), email))
    return emails
