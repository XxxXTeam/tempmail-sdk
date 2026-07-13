"""
Inboxes 渠道 -- https://inboxes.com
"""

import random
from typing import Any, Dict, List, Optional
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "inboxes"
API_BASE = "https://inboxes.com/api/v2"
DEFAULT_DOMAIN = "blondmail.com"
HEADERS = {
    "Accept": "application/json",
    "Origin": "https://inboxes.com",
    "Referer": "https://inboxes.com/",
    "User-Agent": "Mozilla/5.0",
}


def _random_local() -> str:
    chars = "abcdefghijklmnopqrstuvwxyz0123456789"
    return "sdk" + "".join(random.choice(chars) for _ in range(16))


def _get_domains() -> List[str]:
    resp = tm_http.get(f"{API_BASE}/domain", headers=HEADERS, timeout=15)
    resp.raise_for_status()
    data = resp.json()
    items = data.get("domains") if isinstance(data, dict) else []
    domains = [
        str(item.get("qdn") or "").strip()
        for item in items
        if isinstance(item, dict) and str(item.get("qdn") or "").strip()
    ]
    if not domains:
        raise ValueError("inboxes: no domains")
    return domains


def _pick_domain(domains: List[str], preferred: Optional[str]) -> str:
    wanted = (preferred or "").strip().removeprefix("@").lower()
    if wanted:
        for domain in domains:
            if domain.lower() == wanted:
                return domain
    if DEFAULT_DOMAIN in domains:
        return DEFAULT_DOMAIN
    return domains[0]


def _flatten(raw: Dict[str, Any], recipient: str) -> Dict[str, Any]:
    return {
        "id": raw.get("uid") or "",
        "from": raw.get("sf") or raw.get("f") or "",
        "to": raw.get("ib") or recipient,
        "subject": raw.get("s") or "",
        "text": raw.get("text") or "",
        "html": raw.get("html") or "",
        "preview_text": raw.get("ph") or "",
        "date": raw.get("cr") or "",
        "isRead": bool(raw.get("ru")),
        "attachments": raw.get("at") if isinstance(raw.get("at"), list) else [],
    }


def generate_email(domain: Optional[str] = None) -> EmailInfo:
    domains = _get_domains()
    selected = _pick_domain(domains, domain)
    return EmailInfo(channel=CHANNEL, email=f"{_random_local()}@{selected}")


def _fetch_detail(uid: str) -> Optional[Dict[str, Any]]:
    try:
        resp = tm_http.get(
            f"{API_BASE}/message/{quote(uid, safe='')}", headers=HEADERS, timeout=15
        )
        if not resp.ok:
            return None
        data = resp.json()
        return data if isinstance(data, dict) else None
    except Exception:
        return None


def get_emails(email: str) -> List[Email]:
    address = (email or "").strip()
    if not address:
        raise ValueError("inboxes: empty email")

    resp = tm_http.get(
        f"{API_BASE}/inbox/{quote(address, safe='')}", headers=HEADERS, timeout=15
    )
    resp.raise_for_status()
    data = resp.json()
    rows = data.get("msgs") if isinstance(data, dict) else []
    if not isinstance(rows, list):
        return []

    emails: List[Email] = []
    for row in rows:
        if not isinstance(row, dict):
            continue
        uid = str(row.get("uid") or "").strip()
        detail = _fetch_detail(uid) if uid else None
        emails.append(normalize_email(_flatten(detail or row, address), address))
    return emails
