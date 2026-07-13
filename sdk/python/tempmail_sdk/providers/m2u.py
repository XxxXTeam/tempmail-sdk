"""
MailToYou / m2u 渠道
"""

import json
from typing import Any, Dict, List, Optional
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "m2u"
API_BASE = "https://api.m2u.io"
HEADERS = {
    "Accept": "application/json",
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
}


def _pack_token(token: str, view_token: str) -> str:
    return json.dumps({"token": token, "viewToken": view_token})


def _unpack_token(token: str) -> tuple[str, str]:
    value = (token or "").strip()
    if not value:
        return "", ""
    if value.startswith("{"):
        try:
            data = json.loads(value)
            if isinstance(data, dict):
                return (
                    str(data.get("token") or "").strip(),
                    str(data.get("viewToken") or "").strip(),
                )
        except Exception:
            return "", ""
    return value, ""


def _flatten_message(raw: Dict[str, Any], recipient: str) -> Dict[str, Any]:
    return {
        "id": raw.get("id") or raw.get("message_id"),
        "from": raw.get("from_addr")
        or raw.get("from_address")
        or raw.get("from")
        or "",
        "to": recipient,
        "subject": raw.get("subject") or "",
        "text": raw.get("text_body") or raw.get("body_text") or raw.get("text") or "",
        "html": raw.get("html_body") or raw.get("body_html") or raw.get("html") or "",
        "date": raw.get("received_at") or raw.get("created_at") or "",
        "is_read": raw.get("is_read") or raw.get("isRead") or raw.get("seen") or False,
        "attachments": raw.get("attachments") or [],
    }


def generate_email() -> EmailInfo:
    resp = tm_http.post(
        f"{API_BASE}/v1/mailboxes/auto",
        headers={**HEADERS, "Content-Type": "application/json"},
        json={},
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    mailbox = data.get("mailbox") if isinstance(data, dict) else None
    if not isinstance(mailbox, dict):
        raise ValueError("m2u: invalid mailbox response")
    local_part = str(mailbox.get("local_part") or "").strip()
    domain = str(mailbox.get("domain") or "").strip()
    token = str(mailbox.get("token") or "").strip()
    view_token = str(mailbox.get("view_token") or "").strip()
    email = f"{local_part}@{domain}" if local_part and domain else ""
    if not email or not token or not view_token:
        raise ValueError("m2u: invalid mailbox response")

    return EmailInfo(
        channel=CHANNEL,
        email=email,
        _token=_pack_token(token, view_token),
        expires_at=mailbox.get("expires_at"),
        created_at=mailbox.get("created_at"),
    )


def _fetch_detail(
    token: str, view_token: str, message_id: str
) -> Optional[Dict[str, Any]]:
    mailbox_token = quote(token, safe="")
    encoded_message_id = quote(message_id, safe="")
    resp = tm_http.get(
        f"{API_BASE}/v1/mailboxes/{mailbox_token}/messages/{encoded_message_id}",
        params={"view": view_token},
        headers=HEADERS,
        timeout=15,
    )
    if not resp.ok:
        return None
    data = resp.json()
    if not isinstance(data, dict):
        return None
    detail = data.get("message")
    return detail if isinstance(detail, dict) else None


def get_emails(token: str, email: str) -> List[Email]:
    mailbox_token, view_token = _unpack_token(token)
    address = (email or "").strip()
    if not mailbox_token:
        raise ValueError("m2u: missing token")
    if not view_token:
        raise ValueError("m2u: missing view token")
    if not address:
        raise ValueError("m2u: empty email")

    resp = tm_http.get(
        f"{API_BASE}/v1/mailboxes/{quote(mailbox_token, safe='')}/messages",
        params={"view": view_token},
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
        message_id = str(item.get("id") or item.get("message_id") or "").strip()
        detail = (
            _fetch_detail(mailbox_token, view_token, message_id) if message_id else None
        )
        emails.append(
            normalize_email(_flatten_message(detail or item, address), address)
        )
    return emails
