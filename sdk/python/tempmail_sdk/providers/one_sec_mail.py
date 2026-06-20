"""
1SecMail 渠道 -- https://1sec-mail.com
"""

import json
import re
from typing import Any, Dict, List, Tuple

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "1sec-mail"
BASE_URL = "https://1sec-mail.com/"
CSRF_RE = re.compile(r'<meta name="csrf-token" content="([^"]+)"', re.I)


def _merge_cookie(prev: str, resp: Any) -> str:
    jar: Dict[str, str] = {}
    for part in (prev or "").split(";"):
        p = part.strip()
        if "=" in p:
            k, v = p.split("=", 1)
            jar[k] = v
    for cookie in resp.cookies:
        jar[cookie.name] = cookie.value
    return "; ".join(f"{k}={v}" for k, v in jar.items())


def _encode_session(csrf: str, cookie: str) -> str:
    return json.dumps({"csrf": csrf, "cookie": cookie}, separators=(",", ":"))


def _decode_session(token: str) -> Dict[str, str]:
    data = json.loads(token or "{}")
    csrf = str(data.get("csrf") or "").strip()
    cookie = str(data.get("cookie") or "").strip()
    if not csrf or not cookie:
        raise ValueError("1sec-mail: invalid session token")
    return {"csrf": csrf, "cookie": cookie}


def _open_session() -> Dict[str, str]:
    resp = tm_http.get(
        BASE_URL,
        headers={"Accept": "text/html,application/xhtml+xml", "User-Agent": "Mozilla/5.0"},
        timeout=15,
    )
    resp.raise_for_status()
    m = CSRF_RE.search(resp.text)
    if not m:
        raise ValueError("1sec-mail: csrf token not found")
    return {"csrf": m.group(1), "cookie": _merge_cookie("", resp)}


def _fetch_messages(session: Dict[str, str]) -> Tuple[Dict[str, Any], str]:
    resp = tm_http.post(
        f"{BASE_URL}get_messages",
        json={"_token": session["csrf"], "captcha": ""},
        headers={
            "Accept": "application/json, text/plain, */*",
            "Content-Type": "application/json",
            "X-Requested-With": "XMLHttpRequest",
            "X-CSRF-TOKEN": session["csrf"],
            "Referer": BASE_URL,
            "Cookie": session["cookie"],
            "User-Agent": "Mozilla/5.0",
        },
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, dict):
        raise ValueError("1sec-mail: invalid messages response")
    return data, _merge_cookie(session["cookie"], resp)


def _flatten(raw: Dict[str, Any], recipient: str) -> Dict[str, Any]:
    out = dict(raw)
    content = raw.get("content") if isinstance(raw.get("content"), str) else ""
    html = content if raw.get("html") is True else (raw.get("html") if isinstance(raw.get("html"), str) else "")
    out["id"] = raw.get("id")
    out["from"] = raw.get("from_email") or raw.get("from") or ""
    out["to"] = raw.get("to") or recipient
    out["subject"] = raw.get("subject") or ""
    out["text"] = "" if raw.get("html") is True else content
    out["html"] = html
    out["date"] = raw.get("receivedAt") or ""
    out["isRead"] = raw.get("is_seen")
    out["attachments"] = raw.get("attachments")
    return out


def generate_email() -> EmailInfo:
    session = _open_session()
    data, cookie = _fetch_messages(session)
    email = str(data.get("mailbox") or "").strip()
    if not email or "@" not in email:
        raise ValueError("1sec-mail: invalid mailbox response")
    return EmailInfo(channel=CHANNEL, email=email, _token=_encode_session(session["csrf"], cookie))


def get_emails(token: str, email: str) -> List[Email]:
    session = _decode_session(token)
    address = (email or "").strip()
    if not address:
        raise ValueError("1sec-mail: empty email")
    data, _ = _fetch_messages(session)
    rows = data.get("messages")
    if not isinstance(rows, list):
        return []
    return [normalize_email(_flatten(item, address), address) for item in rows if isinstance(item, dict)]
