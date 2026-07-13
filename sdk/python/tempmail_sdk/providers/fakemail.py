"""
FakeMail 渠道 -- https://www.fakemail.net
"""

import json
import re
from typing import Any, Dict, List, Optional

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "fakemail"
BASE_URL = "https://www.fakemail.net"
CSRF_RE = re.compile(r'const\s+CSRF\s*=\s*"([^"]+)"')


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


def _json(text: str) -> Any:
    return json.loads((text or "").lstrip("\ufeff"))


def _home_headers() -> Dict[str, str]:
    return {"Accept": "text/html,application/xhtml+xml", "User-Agent": "Mozilla/5.0"}


def _ajax_headers(cookie: str) -> Dict[str, str]:
    return {
        "Accept": "application/json, text/javascript, */*; q=0.01",
        "X-Requested-With": "XMLHttpRequest",
        "Referer": f"{BASE_URL}/",
        "Cookie": cookie,
        "User-Agent": "Mozilla/5.0",
    }


def _open_session() -> Dict[str, str]:
    resp = tm_http.get(f"{BASE_URL}/", headers=_home_headers(), timeout=15)
    resp.raise_for_status()
    m = CSRF_RE.search(resp.text)
    if not m:
        raise ValueError("fakemail: csrf token not found")
    return {"csrf": m.group(1), "cookie": _merge_cookie("", resp)}


def _create_address(session: Dict[str, str]) -> Dict[str, Any]:
    resp = tm_http.get(
        f"{BASE_URL}/index/index",
        params={"csrf_token": session["csrf"]},
        headers=_ajax_headers(session["cookie"]),
        timeout=15,
    )
    resp.raise_for_status()
    session["cookie"] = _merge_cookie(session["cookie"], resp)
    data = _json(resp.text)
    if not isinstance(data, dict):
        raise ValueError("fakemail: invalid mailbox response")
    return data


def _fetch_rows(cookie: str) -> List[Dict[str, Any]]:
    resp = tm_http.get(
        f"{BASE_URL}/index/refresh", headers=_ajax_headers(cookie), timeout=15
    )
    resp.raise_for_status()
    data = _json(resp.text)
    return data if isinstance(data, list) else []


def _fetch_detail(cookie: str, message_id: str) -> Optional[Dict[str, Any]]:
    try:
        resp = tm_http.post(
            f"{BASE_URL}/index/email",
            data={"id": message_id},
            headers=_ajax_headers(cookie),
            timeout=15,
        )
        if not resp.ok:
            return None
        data = _json(resp.text)
        return data if isinstance(data, dict) else None
    except Exception:
        return None


def _flatten(
    row: Dict[str, Any], detail: Optional[Dict[str, Any]], recipient: str
) -> Dict[str, Any]:
    detail = detail or {}
    return {
        "id": detail.get("id") or row.get("id"),
        "from": detail.get("od") or row.get("od") or "",
        "to": recipient,
        "subject": detail.get("predmet") or row.get("predmet") or "",
        "text": "",
        "html": detail.get("telo") or "",
        "date": row.get("kdy") or "",
        "isRead": row.get("precteno") == "precteno",
    }


def generate_email() -> EmailInfo:
    session = _open_session()
    data = _create_address(session)
    email = str(data.get("email") or "").strip()
    if not email or "@" not in email:
        raise ValueError("fakemail: invalid mailbox response")
    return EmailInfo(channel=CHANNEL, email=email, _token=session["cookie"])


def get_emails(token: str, email: str) -> List[Email]:
    cookie = (token or "").strip()
    address = (email or "").strip()
    if not cookie:
        raise ValueError("fakemail: empty session token")
    if not address:
        raise ValueError("fakemail: empty email")
    emails: List[Email] = []
    for row in _fetch_rows(cookie):
        if not isinstance(row, dict):
            continue
        message_id = str(row.get("id") or "").strip()
        detail = _fetch_detail(cookie, message_id) if message_id else None
        emails.append(normalize_email(_flatten(row, detail, address), address))
    return emails
