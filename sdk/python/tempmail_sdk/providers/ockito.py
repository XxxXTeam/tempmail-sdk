"""
ockito 渠道实现
"""

import json
from typing import Any, Dict, List, Optional, Tuple
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "ockito"
BASE_URL = "https://ockito.com/web-api"

DEFAULT_HEADERS = {
    "Accept": "application/json",
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
}


def _fetch_json(path: str, *, method: str = "GET", headers: Optional[Dict[str, str]] = None, payload: Optional[Dict[str, Any]] = None) -> Tuple[int, Dict[str, Any]]:
    request_headers = dict(DEFAULT_HEADERS)
    if headers:
        request_headers.update(headers)
    if method == "POST":
        resp = tm_http.post(f"{BASE_URL}{path}", headers=request_headers, json=payload or {})
    else:
        resp = tm_http.get(f"{BASE_URL}{path}", headers=request_headers)
    text = resp.text or ""
    try:
        data = json.loads(text) if text else {}
    except Exception as exc:
        raise ValueError(f"ockito invalid JSON: {path} HTTP {resp.status_code}") from exc
    if not resp.ok:
        raise ValueError(f"ockito http {resp.status_code}")
    return resp.status_code, data if isinstance(data, dict) else {}


def _pack_token(access_token: str, refresh_token: str) -> str:
    return json.dumps({"access_token": access_token, "refresh_token": refresh_token})


def _unpack_token(token: str) -> tuple[str, str]:
    value = (token or "").strip()
    if not value or not value.startswith("{"):
        raise ValueError("ockito: invalid session token")
    data = json.loads(value)
    if not isinstance(data, dict):
        raise ValueError("ockito: invalid session token")
    access_token = str(data.get("access_token") or "").strip()
    refresh_token = str(data.get("refresh_token") or "").strip()
    if not access_token or not refresh_token:
        raise ValueError("ockito: invalid session token")
    return access_token, refresh_token


def _refresh_access_token(refresh_token: str) -> str:
    status, data = _fetch_json(
        "/grefresh",
        method="POST",
        headers={
            "Authorization": f"Bearer {refresh_token}",
            "X-PASSTHROUGH": "Y",
        },
    )
    if not 200 <= status < 300:
        raise ValueError(f"ockito grefresh http {status}")
    access_token = str(data.get("access_token") or "").strip()
    if not access_token:
        raise ValueError("ockito: invalid refresh response")
    return access_token


def _fetch_bearer_json(path: str, access_token: str, refresh_token: str) -> Dict[str, Any]:
    try:
        _, data = _fetch_json(path, headers={"Authorization": f"Bearer {access_token}"})
        return data
    except ValueError as exc:
        if "http 401" not in str(exc):
            raise
    refreshed = _refresh_access_token(refresh_token)
    _, data = _fetch_json(path, headers={"Authorization": f"Bearer {refreshed}"})
    return data


def _flatten_inbox_row(raw: Dict[str, Any], recipient: str) -> Dict[str, Any]:
    return {
        "id": raw.get("uid") or "",
        "from": raw.get("sender") or "",
        "to": recipient,
        "subject": raw.get("subject") or "",
        "text": raw.get("snippet") or "",
        "html": raw.get("html") or "",
        "date": raw.get("timestamp") or "",
        "is_read": False,
        "attachments": [],
    }


def _flatten_message(raw: Dict[str, Any], recipient: str) -> Dict[str, Any]:
    obj = raw.get("obj") if isinstance(raw.get("obj"), dict) else raw
    return {
        "id": raw.get("uid") or "",
        "from": obj.get("sender_email") or obj.get("SenderEmail") or obj.get("from_") or obj.get("From") or obj.get("from") or obj.get("sender_name") or obj.get("SenderName") or "",
        "to": obj.get("to") or obj.get("To") or recipient,
        "subject": obj.get("subject") or obj.get("Subject") or "",
        "text": obj.get("text") or "",
        "html": obj.get("html") or "",
        "date": obj.get("date") or obj.get("Date") or "",
        "is_read": False,
        "attachments": [],
    }


def generate_email() -> EmailInfo:
    _, login = _fetch_json("/gtoken", method="POST", payload={})
    access_token = str(login.get("access_token") or "").strip()
    refresh_token = str(login.get("refresh_token") or "").strip()
    if not access_token or not refresh_token:
        raise ValueError("ockito: invalid token response")

    _, email_data = _fetch_json("/email", headers={"Authorization": f"Bearer {access_token}"})
    email_value = email_data.get("email")
    email = ""
    if isinstance(email_value, str):
        email = email_value.strip()
    elif isinstance(email_value, dict):
        email = str(email_value.get("email") or "").strip()
    if not email or "@" not in email:
        raise ValueError("ockito: invalid email response")

    return EmailInfo(channel=CHANNEL, email=email, _token=_pack_token(access_token, refresh_token))


def get_emails(token: str, email: str) -> List[Email]:
    access_token, refresh_token = _unpack_token(token)
    address = (email or "").strip()
    if not address:
        raise ValueError("ockito: empty email")

    data = _fetch_bearer_json(f"/retrieve/{quote(address, safe='')}/emails", access_token, refresh_token)
    rows = data.get("inbox") or []
    if not isinstance(rows, list):
        return []

    out: List[Email] = []
    for row in rows:
        if not isinstance(row, dict):
            continue
        uid = str(row.get("uid") or "").strip()
        if not uid:
            out.append(normalize_email(_flatten_inbox_row(row, address), address))
            continue
        try:
            detail = _fetch_bearer_json(
                f"/retrieve/{quote(address, safe='')}/{quote(uid, safe='')}",
                access_token,
                refresh_token,
            )
            out.append(normalize_email(_flatten_message(detail, address), address))
        except Exception:
            out.append(normalize_email(_flatten_inbox_row(row, address), address))
    return out
