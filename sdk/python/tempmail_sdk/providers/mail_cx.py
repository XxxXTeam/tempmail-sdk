"""
mail.cx 渠道实现（公开 REST API，见 https://docs.mail.cx）
流程: GET /api/domains → POST /api/accounts（返回 JWT）→ GET /api/messages
"""

import random
import string
from typing import Any, Dict, List, Optional

from ..types import EmailInfo
from ..normalize import normalize_email
from .. import http as tm_http

CHANNEL = "mail-cx"
BASE = "https://api.mail.cx"

DEFAULT_HEADERS = {
    "Accept": "application/json",
    "Origin": "https://mail.cx",
    "Referer": "https://mail.cx/",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36"
    ),
}


def _random_str(n: int) -> str:
    chars = string.ascii_lowercase + string.digits
    return "".join(random.choices(chars, k=n))


def _get_domains() -> List[str]:
    r = tm_http.get(f"{BASE}/api/domains", headers=DEFAULT_HEADERS)
    r.raise_for_status()
    data = r.json()
    domains = data.get("domains") or []
    if not isinstance(domains, list):
        return []
    out = []
    for d in domains:
        if isinstance(d, dict) and d.get("domain"):
            out.append(str(d["domain"]))
    return out


def _create_account(address: str, password: str) -> Dict[str, Any]:
    h = {**DEFAULT_HEADERS, "Content-Type": "application/json"}
    r = tm_http.post(
        f"{BASE}/api/accounts",
        headers=h,
        json={"address": address, "password": password},
    )
    if r.status_code != 201:
        raise RuntimeError(f"mail-cx create account: {r.status_code} {r.text}")
    data = r.json()
    if not data.get("token") or not data.get("address"):
        raise RuntimeError("mail-cx: invalid account response")
    return data


def generate_email(domain: Optional[str] = None) -> EmailInfo:
    domains = _get_domains()
    if not domains:
        raise RuntimeError("No available domains")
    if domain:
        want = domain.lower().lstrip("@")
        matched = [d for d in domains if d.lower() == want]
        if matched:
            domains = matched

    last_err: Optional[Exception] = None
    for _ in range(8):
        d = random.choice(domains)
        address = f"{_random_str(12)}@{d}"
        password = _random_str(16)
        try:
            acc = _create_account(address, password)
            return EmailInfo(
                channel=CHANNEL,
                email=acc["address"],
                _token=acc["token"],
            )
        except RuntimeError as e:
            last_err = e
            if "409" in str(e):
                continue
            raise
    if last_err:
        raise last_err
    raise RuntimeError("mail-cx: could not create account")


def _flatten_message(msg: Dict[str, Any], recipient_email: str) -> Dict[str, Any]:
    mid = str(msg.get("id", ""))
    attachments: List[Dict[str, Any]] = []
    raw_att = msg.get("attachments")
    if isinstance(raw_att, list):
        for a in raw_att:
            if not isinstance(a, dict):
                continue
            idx = a.get("index")
            attachments.append(
                {
                    "filename": a.get("filename", ""),
                    "size": a.get("size"),
                    "content_type": a.get("content_type"),
                    "url": f"{BASE}/api/messages/{mid}/attachments/{idx}",
                }
            )
    return {
        "id": mid,
        "sender": msg.get("sender", ""),
        "from": msg.get("from", ""),
        "address": msg.get("address") or recipient_email,
        "subject": msg.get("subject", ""),
        "preview_text": msg.get("preview_text"),
        "text_body": msg.get("text_body"),
        "html_body": msg.get("html_body"),
        "created_at": msg.get("created_at"),
        "attachments": attachments,
    }


def get_emails(token: str, email: str = "", **_kwargs: Any) -> List[Any]:
    h = {**DEFAULT_HEADERS, "Authorization": f"Bearer {token}"}
    r = tm_http.get(f"{BASE}/api/messages?page=1", headers=h)
    r.raise_for_status()
    data = r.json()
    messages = data.get("messages") or []
    if not isinstance(messages, list) or not messages:
        return []

    out = []
    for msg in messages:
        if not isinstance(msg, dict):
            continue
        mid = msg.get("id")
        flat_source = msg
        if mid:
            try:
                dr = tm_http.get(f"{BASE}/api/messages/{mid}", headers=h)
                if dr.status_code == 200:
                    detail = dr.json()
                    if isinstance(detail, dict):
                        flat_source = detail
            except Exception:
                pass
        flat = _flatten_message(flat_source, email)
        out.append(normalize_email(flat, email))
    return out
