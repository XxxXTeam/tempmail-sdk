"""
WebMailTemp 渠道 -- https://webmailtemp.com
"""

import base64
import json
import time
from typing import Any, Dict, List
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "webmailtemp"
BASE_URL = "https://webmailtemp.com"
HEADERS = {"Accept": "application/json", "User-Agent": "Mozilla/5.0"}


def _encode_token(username: str, cookie: str) -> str:
    raw = json.dumps({"u": username, "c": cookie}, separators=(",", ":")).encode()
    return "wmt1:" + base64.b64encode(raw).decode()


def _decode_token(token: str) -> Dict[str, str]:
    if not token.startswith("wmt1:"):
        raise ValueError("webmailtemp: invalid token")
    data = json.loads(base64.b64decode(token[5:]).decode())
    username = str(data.get("u") or "").strip()
    cookie = str(data.get("c") or "").strip()
    if not username or not cookie:
        raise ValueError("webmailtemp: invalid token data")
    return {"username": username, "cookie": cookie}


def _flatten(raw: Dict[str, Any], recipient: str) -> Dict[str, Any]:
    return {
        "id": raw.get("id"),
        "from": raw.get("from"),
        "to": recipient,
        "subject": raw.get("subject"),
        "text": raw.get("body"),
        "html": raw.get("html"),
        "date": raw.get("received_at") or raw.get("timestamp"),
        "attachments": raw.get("attachments"),
    }


def generate_email() -> EmailInfo:
    resp = tm_http.get(f"{BASE_URL}/api/create", headers=HEADERS, timeout=15)
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, dict):
        raise ValueError("webmailtemp: invalid create response")
    email = str(data.get("email") or "").strip()
    username = str(data.get("username") or "").strip()
    cookie = next(iter(resp.cookies.items()), None)
    cookie_header = f"{cookie[0]}={cookie[1]}" if cookie else ""
    if not data.get("success") or not email or "@" not in email or not username or not cookie_header:
        raise ValueError("webmailtemp: invalid create response")
    ttl = data.get("ttl")
    expires_at = int((time.time() + float(ttl)) * 1000) if isinstance(ttl, (int, float)) and ttl > 0 else None
    return EmailInfo(channel=CHANNEL, email=email, _token=_encode_token(username, cookie_header), expires_at=expires_at)


def get_emails(token: str, email: str) -> List[Email]:
    address = (email or "").strip()
    if not address:
        raise ValueError("webmailtemp: empty email")
    session = _decode_token(token)
    headers = dict(HEADERS)
    headers["Cookie"] = session["cookie"]
    resp = tm_http.get(
        f"{BASE_URL}/api/check/{quote(session['username'], safe='')}",
        headers=headers,
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    rows = data.get("emails") if isinstance(data, dict) else []
    if not isinstance(rows, list):
        return []
    return [normalize_email(_flatten(row, address), address) for row in rows if isinstance(row, dict)]
