"""
Neighbours 渠道 -- https://neighbours.sh
"""

import random
import string
from typing import Any, Dict, List, Optional
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "neighbours"
API_BASE = "https://neighbours.sh/api/v1"
HEADERS = {
    "Accept": "application/json",
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
}


def _request_json(path: str, allow_404: bool = False) -> Optional[Dict[str, Any]]:
    resp = tm_http.get(f"{API_BASE}{path}", headers=HEADERS, timeout=15)
    if allow_404 and resp.status_code == 404:
        return None
    resp.raise_for_status()
    data = resp.json()
    return data if isinstance(data, dict) else None


def _random_int(limit: int) -> int:
    if limit <= 1:
        return 0
    return random.randrange(limit)


def _random_local() -> str:
    chars = string.ascii_lowercase + string.digits
    return "sdk" + "".join(random.choice(chars) for _ in range(16))


def _first_address(value: Any) -> str:
    if value is None:
        return ""
    if isinstance(value, str):
        return value.strip()
    if isinstance(value, list):
        for item in value:
            hit = _first_address(item)
            if hit:
                return hit
        return ""
    if isinstance(value, dict):
        address = str(value.get("address") or "").strip()
        if address:
            return address
        text = str(value.get("text") or "").strip()
        if "@" in text:
            return text
        return _first_address(value.get("value"))
    return str(value).strip()


def _flatten_message(raw: Dict[str, Any], recipient: str) -> Dict[str, Any]:
    text = str(raw.get("text") or raw.get("snippet") or "")
    date = str(raw.get("date") or raw.get("received_at") or "")
    return {
        "id": str(raw.get("uid") or ""),
        "from": _first_address(raw.get("from")),
        "to": _first_address(raw.get("to")) or recipient,
        "subject": str(raw.get("subject") or ""),
        "text": text,
        "html": str(raw.get("html") or ""),
        "date": date,
        "is_read": False,
        "attachments": raw.get("attachments") or [],
    }


def _list_domains() -> List[str]:
    data = _request_json("/config/domains")
    domains = []
    if isinstance(data, dict):
        nested = data.get("data")
        if isinstance(nested, dict):
            for item in nested.get("domains", []):
                item_str = str(item).strip().lower()
                if item_str:
                    domains.append(item_str)
    if not domains:
        raise ValueError("neighbours: domain list is empty")
    return domains


def _pick_domain(domains: List[str], preferred: Optional[str]) -> str:
    if preferred and preferred.strip():
        wanted = preferred.strip().lower()
        if wanted not in domains:
            raise ValueError(f"neighbours: unsupported domain {preferred}")
        return wanted
    return domains[_random_int(len(domains))]


def _fetch_detail(address: str, uid: str) -> Optional[Dict[str, Any]]:
    data = _request_json(
        f"/inbox/{quote(address, safe='')}/{quote(uid, safe='')}", allow_404=True
    )
    if isinstance(data, dict):
        detail = data.get("data")
        if isinstance(detail, dict):
            return detail
    return None


def generate_email(domain: Optional[str] = None) -> EmailInfo:
    domains = _list_domains()
    selected = _pick_domain(domains, domain)
    email = f"{_random_local()}@{selected}"
    return EmailInfo(channel=CHANNEL, email=email, _token=email)


def get_emails(email: str) -> List[Email]:
    address = (email or "").strip()
    if not address:
        raise ValueError("neighbours: empty email")

    data = _request_json(f"/inbox/{quote(address, safe='')}", allow_404=True)
    if not data:
        return []
    rows = data.get("data") if isinstance(data, dict) else []
    if not isinstance(rows, list):
        return []

    emails: List[Email] = []
    for item in rows:
        if not isinstance(item, dict):
            continue
        uid = str(item.get("uid") or "").strip()
        detail = _fetch_detail(address, uid) if uid else None
        emails.append(
            normalize_email(_flatten_message(detail or item, address), address)
        )
    return emails
