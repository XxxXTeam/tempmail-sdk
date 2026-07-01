"""
fmail 渠道实现
"""

from typing import Any, Dict, List, Optional
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "fmail"
BASE_URL = "https://fmail.men"

DEFAULT_HEADERS = {
    "Accept": "application/json",
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
}


def _fetch_json(path: str) -> Dict[str, Any]:
    resp = tm_http.get(f"{BASE_URL}{path}", headers=DEFAULT_HEADERS)
    resp.raise_for_status()
    data = resp.json()
    return data if isinstance(data, dict) else {}


def _normalize_domain(domain: Optional[str]) -> Optional[str]:
    value = str(domain or "").strip().lstrip("@")
    return value or None


def _flatten_message(raw: Dict[str, Any], recipient: str) -> Dict[str, Any]:
    return {
        "id": raw.get("id") or raw.get("uid") or raw.get("token") or "",
        "from": raw.get("sender") or raw.get("from") or "",
        "to": raw.get("recipient") or raw.get("to") or recipient,
        "subject": raw.get("subject") or "",
        "text": raw.get("body_text") or raw.get("text") or raw.get("snippet") or "",
        "html": raw.get("body_html") or raw.get("html") or "",
        "date": raw.get("received_at") or raw.get("timestamp") or raw.get("date") or "",
        "is_read": raw.get("is_read") or False,
        "attachments": raw.get("attachments") or [],
    }


def generate_email(domain: Optional[str] = None) -> EmailInfo:
    selected = _normalize_domain(domain)
    path = "/v1/random"
    if selected:
        path = f"{path}?domain={quote(selected, safe='')}"
    data = _fetch_json(path)
    email = str(data.get("address") or "").strip()
    if not email:
        username = str(data.get("username") or "").strip()
        dom = str(data.get("domain") or "").strip()
        if username and dom:
            email = f"{username}@{dom}"
    if not email or "@" not in email:
        raise ValueError("fmail: invalid random response")
    return EmailInfo(channel=CHANNEL, email=email)


def get_emails(email: str) -> List[Email]:
    value = (email or "").strip()
    local, _, domain = value.partition("@")
    if not local or not domain:
        raise ValueError("fmail: invalid email")

    data = _fetch_json(f"/v1/inbox/{quote(local, safe='')}?domain={quote(domain, safe='')}&limit=50")
    rows = data.get("emails") or []
    if not isinstance(rows, list):
        return []

    out: List[Email] = []
    for row in rows:
        if not isinstance(row, dict):
            continue
        token = str(row.get("token") or row.get("id") or "").strip()
        if not token:
            out.append(normalize_email(_flatten_message(row, value), value))
            continue
        try:
            detail = _fetch_json(f"/v1/email/{quote(token, safe='')}")
            nested = detail.get("email") if isinstance(detail.get("email"), dict) else detail
            out.append(normalize_email(_flatten_message(nested if isinstance(nested, dict) else row, value), value))
        except Exception:
            out.append(normalize_email(_flatten_message(row, value), value))
    return out
