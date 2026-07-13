"""
GetNada 渠道 -- https://getnada.net
"""

import random
import re
from typing import Any, Dict, List, Optional
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "getnada"
API_BASE = "https://getnada.net/api"
HEADERS_JSON = {"Accept": "application/json"}
HEADERS_POST = {"Accept": "application/json", "Content-Type": "application/json"}
_DOMAIN_LABEL_RE = re.compile(r"^[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$", re.I)


def _random_local() -> str:
    chars = "abcdefghijklmnopqrstuvwxyz0123456789"
    return "sdk" + "".join(random.choice(chars) for _ in range(16))


def _pick_domain(preferred: Optional[str] = None) -> str:
    resp = tm_http.get(f"{API_BASE}/public/domains", headers=HEADERS_JSON, timeout=15)
    resp.raise_for_status()
    data = resp.json()
    domains = data.get("domains") if isinstance(data, dict) else []
    if not isinstance(domains, list):
        domains = []
    cleaned = [_clean_domain(domain) for domain in domains]
    cleaned = [domain for domain in cleaned if domain]
    wanted = str(preferred or "").strip().lstrip("@").lower()
    if wanted:
        for domain in cleaned:
            if domain == wanted:
                return domain
        raise ValueError(f"getnada: domain not available: {wanted}")
    for domain in cleaned:
        if domain == "getnada.net":
            return domain
    if cleaned:
        return cleaned[0]
    raise ValueError("getnada: no domain available")


def _clean_domain(value: Any) -> str:
    domain = str(value or "").strip().lower()
    if not domain or len(domain) > 253 or ".." in domain:
        return ""
    labels = domain.split(".")
    if len(labels) < 2:
        return ""
    if not all(_DOMAIN_LABEL_RE.fullmatch(label) for label in labels):
        return ""
    return domain


def _flatten_message(raw: Dict[str, Any], recipient: str) -> Dict[str, Any]:
    out = dict(raw)
    out["id"] = raw.get("id")
    out["from"] = raw.get("from_addr") or raw.get("from") or ""
    out["to"] = raw.get("to") or recipient
    out["text"] = raw.get("text_plain") or raw.get("text") or ""
    out["html"] = raw.get("html_sanitized") or raw.get("html") or ""
    if "read_at" in raw:
        out["read"] = bool(raw.get("read_at"))
    return out


def generate_email(domain: Optional[str] = None, channel: str = CHANNEL) -> EmailInfo:
    selected_domain = _pick_domain(domain)
    requested = f"{_random_local()}@{selected_domain}"
    resp = tm_http.post(
        f"{API_BASE}/inbox/open",
        headers=HEADERS_POST,
        json={"email": requested},
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, dict):
        raise ValueError("getnada: invalid open response")
    token = str(data.get("token") or "").strip()
    email = str(data.get("recipient") or requested).strip()
    if not token or not email or "@" not in email:
        raise ValueError("getnada: invalid open response")
    return EmailInfo(
        channel=channel,
        email=email,
        _token=token,
        expires_at=data.get("activeUntil"),
    )


def _fetch_detail(token: str, message_id: str) -> Optional[Dict[str, Any]]:
    resp = tm_http.get(
        f"{API_BASE}/inbox/message?id={quote(message_id, safe='')}&token={quote(token, safe='')}",
        headers=HEADERS_JSON,
        timeout=15,
    )
    if not resp.ok:
        return None
    data = resp.json()
    if isinstance(data, dict) and isinstance(data.get("message"), dict):
        return data["message"]
    return None


def get_emails(token: str, email: str) -> List[Email]:
    auth = (token or "").strip()
    address = (email or "").strip()
    if not auth:
        raise ValueError("getnada: empty token")
    if not address:
        raise ValueError("getnada: empty email")

    resp = tm_http.get(
        f"{API_BASE}/inbox/messages?token={quote(auth, safe='')}",
        headers=HEADERS_JSON,
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
        detail = _fetch_detail(auth, message_id) if message_id else None
        emails.append(
            normalize_email(_flatten_message(detail or item, address), address)
        )
    return emails
