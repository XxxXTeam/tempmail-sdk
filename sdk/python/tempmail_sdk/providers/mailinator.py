"""
Mailinator 渠道实现
API: https://mailinator.com
"""

from datetime import datetime, timezone
from typing import Any, Dict, List, Optional

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "mailinator"
BASE_URL = "https://mailinator.com"
PUBLIC_DOMAIN = "public"

HEADERS = {
    "Accept": "application/json",
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "Cache-Control": "no-cache",
    "Pragma": "no-cache",
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
}


def _random_string(length: int) -> str:
    import random
    import string

    return "".join(random.choices(string.ascii_lowercase + string.digits, k=length))


def _parse_messages(data: Any) -> List[Dict[str, Any]]:
    if isinstance(data, list):
        return [item for item in data if isinstance(item, dict)]
    if isinstance(data, dict):
        for key in ("msgs", "data"):
            value = data.get(key)
            if isinstance(value, list):
                return [item for item in value if isinstance(item, dict)]
    return []


def _to_iso_time(value: Any) -> str:
    if value in (None, ""):
        return ""
    if isinstance(value, (int, float)):
        millis = int(value if value > 1e12 else value * 1000)
        return datetime.fromtimestamp(millis / 1000, tz=timezone.utc).isoformat().replace("+00:00", "Z")
    text = str(value)
    try:
        parsed = datetime.fromisoformat(text.replace("Z", "+00:00"))
        return parsed.astimezone(timezone.utc).isoformat().replace("+00:00", "Z")
    except Exception:
        return text


def _text_from_payload(data: Any, key: str) -> str:
    if isinstance(data, dict):
        value = data.get(key)
        if value is None:
            return ""
        return value if isinstance(value, str) else str(value)
    return ""


def _attachment_url(value: Any) -> str:
    if not isinstance(value, str) or not value:
        return ""
    if value.startswith("http://") or value.startswith("https://"):
        return value
    return f"{BASE_URL}{value}"


def _request_json(path: str) -> Optional[Dict[str, Any]]:
    try:
        resp = tm_http.get(path, headers=HEADERS, timeout=15)
        if not resp.ok:
            return None
        data = resp.json()
        return data if isinstance(data, dict) else {"data": data}
    except Exception:
        return None


def _flatten_message(
    summary: Dict[str, Any],
    recipient_email: str,
    text_payload: Optional[Dict[str, Any]],
    html_payload: Optional[Dict[str, Any]],
    attachments_payload: Optional[Dict[str, Any]],
) -> Dict[str, Any]:
    attachments: List[Dict[str, Any]] = []
    for att in (attachments_payload or {}).get("attachments", []) or []:
        if not isinstance(att, dict):
            continue
        attachments.append(
            {
                "filename": att.get("name") or att.get("filename") or "",
                "size": att.get("size") or att.get("filesize"),
                "contentType": att.get("contentType") or att.get("content_type") or att.get("mimeType") or att.get("mime_type"),
                "url": _attachment_url(att.get("downloadUrl") or att.get("url")),
            }
        )

    return {
        "id": summary.get("id") or summary.get("messageId") or "",
        "from": summary.get("from") or summary.get("origfrom") or "",
        "to": summary.get("to") or recipient_email,
        "subject": summary.get("subject") or "",
        "text": _text_from_payload(text_payload, "text/plain"),
        "html": _text_from_payload(html_payload, "text/html"),
        "date": _to_iso_time(summary.get("time") or summary.get("date")),
        "seen": False,
        "attachments": attachments,
    }


def generate_email() -> EmailInfo:
    local = _random_string(12)
    return EmailInfo(channel=CHANNEL, email=f"{local}@mailinator.com")


def get_emails(token: str, email: str = "", **kwargs) -> List[Email]:
    del token, kwargs
    inbox = (email or "").strip()
    if "@" in inbox:
        inbox = inbox.split("@", 1)[0]
    if not inbox:
        return []

    data = _request_json(f"{BASE_URL}/api/v2/domains/{PUBLIC_DOMAIN}/inboxes/{inbox}")
    messages = _parse_messages(data)
    if not messages:
        return []

    result: List[Email] = []
    for msg in messages:
        message_id = str(msg.get("id") or msg.get("messageId") or "").strip()
        text_payload = _request_json(f"{BASE_URL}/api/v2/domains/{PUBLIC_DOMAIN}/messages/{message_id}/text") if message_id else None
        html_payload = _request_json(f"{BASE_URL}/api/v2/domains/{PUBLIC_DOMAIN}/messages/{message_id}/texthtml") if message_id else None
        attachments_payload = _request_json(f"{BASE_URL}/api/v2/domains/{PUBLIC_DOMAIN}/messages/{message_id}/attachments") if message_id else None
        result.append(
            normalize_email(
                _flatten_message(msg, email, text_payload, html_payload, attachments_payload),
                email,
            )
        )
    return result
