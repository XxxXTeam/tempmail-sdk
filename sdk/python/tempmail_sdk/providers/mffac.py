"""MFFAC — https://www.mffac.com/api"""

from datetime import datetime, timezone
from typing import Any, Dict, Optional
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import EmailInfo, Email

CHANNEL = "mffac"
BASE = "https://www.mffac.com/api"

_HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    "Content-Type": "application/json",
    "Accept": "*/*",
    "Origin": "https://www.mffac.com",
    "Referer": "https://www.mffac.com/",
    "sec-ch-ua": '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
    "sec-ch-ua-mobile": "?0",
    "sec-ch-ua-platform": '"Windows"',
    "sec-fetch-dest": "empty",
    "sec-fetch-mode": "cors",
    "sec-fetch-site": "same-origin",
}

_GET_HEADERS = {k: v for k, v in _HEADERS.items() if k != "Content-Type"}


def _received_at_to_iso(value: Any) -> str:
    try:
        seconds = float(value)
    except (TypeError, ValueError):
        return ""
    if seconds <= 0:
        return ""
    return datetime.fromtimestamp(seconds, tz=timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def _flatten_email(raw: Dict[str, Any], recipient: str) -> Dict[str, Any]:
    return {
        "id": raw.get("id", ""),
        "from": raw.get("fromAddress", ""),
        "to": raw.get("toAddress") or recipient,
        "subject": raw.get("subject", ""),
        "text": raw.get("textContent", ""),
        "html": raw.get("htmlContent", ""),
        "date": _received_at_to_iso(raw.get("receivedAt")),
        "isRead": raw.get("isRead", False),
        "attachments": [],
    }


def _fetch_email_detail(message_id: str) -> Optional[Dict[str, Any]]:
    r = tm_http.get(f"{BASE}/emails/{quote(message_id, safe='')}", headers=_GET_HEADERS)
    if not r.ok:
        return None
    data = r.json()
    email = data.get("email") if isinstance(data, dict) and data.get("success") else None
    return email if isinstance(email, dict) else None


def generate_email(**kwargs) -> EmailInfo:
    r = tm_http.post(f"{BASE}/mailboxes", headers=_HEADERS, json={"expiresInHours": 24})
    r.raise_for_status()
    data = r.json()
    if not data.get("success") or not data.get("mailbox"):
        raise RuntimeError("mffac: create failed")
    mb = data["mailbox"]
    addr = (mb.get("address") or "").strip()
    mid = (mb.get("id") or "").strip()
    if not addr or not mid:
        raise RuntimeError("mffac: invalid mailbox")
    return EmailInfo(
        channel=CHANNEL,
        email=f"{addr}@mffac.com",
        _token=mid,
        expires_at=None,
        created_at=mb.get("createdAt"),
    )


def get_emails(email: str, token: str = "", **kwargs) -> list:
    local = email.split("@", 1)[0].strip() if email else ""
    if not local:
        raise ValueError("mffac: empty address")
    r = tm_http.get(f"{BASE}/mailboxes/{local}/emails", headers=_GET_HEADERS)
    r.raise_for_status()
    data = r.json()
    if not data.get("success"):
        raise RuntimeError("mffac: list failed")
    raw_list = data.get("emails") or []
    out: list = []
    for raw in raw_list:
        if isinstance(raw, dict):
            message_id = str(raw.get("id") or "").strip()
            detail = _fetch_email_detail(message_id) if message_id else None
            out.append(normalize_email(_flatten_email(detail or raw, email), email))
    return out
