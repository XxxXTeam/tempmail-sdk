"""
Temporam 渠道实现
API: https://temporam.com/api/domains, /api/emails
"""

import secrets
import string
from typing import Optional
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "temporam"
ORIGIN = "https://temporam.com"

HEADERS = {
    "Accept": "application/json",
    "Accept-Encoding": "gzip, deflate, br",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"
    ),
}


def _random_local(length: int = 18) -> str:
    chars = string.ascii_lowercase + string.digits
    return "sdk" + "".join(secrets.choice(chars) for _ in range(length))


def _get_json(path: str) -> dict:
    resp = tm_http.get(f"{ORIGIN}{path}", headers=HEADERS)
    resp.raise_for_status()
    return resp.json()


def _domains() -> list:
    data = _get_json("/api/domains")
    domains = [
        (item.get("domain") or "").strip()
        for item in data.get("data", [])
        if isinstance(item, dict)
    ]
    domains = [domain for domain in domains if domain]
    if not domains:
        raise RuntimeError("temporam: domain list is empty")
    return domains


def _normalize(raw: dict, email: str) -> Email:
    flat = {
        **raw,
        "id": raw.get("id") or raw.get("uuid") or "",
        "from": raw.get("fromEmail") or raw.get("from") or "",
        "to": raw.get("toEmail") or raw.get("to") or email,
        "date": raw.get("createdAt") or raw.get("created_at") or raw.get("date") or "",
    }
    return normalize_email(flat, email)


def generate_email(domain: Optional[str] = None, **kwargs) -> EmailInfo:
    """创建临时邮箱"""
    domains = _domains()
    selected = secrets.choice(domains)
    if domain:
        if domain not in domains:
            raise RuntimeError(f"temporam: unsupported domain {domain}")
        selected = domain
    email = f"{_random_local()}@{selected}"
    return EmailInfo(channel=CHANNEL, email=email, _token=email)


def get_emails(token: str = "", email: str = "", **kwargs) -> list:
    """获取邮件列表"""
    recipient = token or email
    if not recipient:
        raise RuntimeError("temporam: missing email")
    data = _get_json(f"/api/emails?email={quote(recipient)}&limit=50")
    rows = data.get("data") or []
    out = []
    for row in rows:
        if not isinstance(row, dict):
            continue
        raw = row
        msg_id = row.get("id") or row.get("uuid") or ""
        if msg_id:
            try:
                detail = _get_json(f"/api/emails/{quote(str(msg_id), safe='')}")
                if isinstance(detail.get("data"), dict):
                    raw = detail["data"]
            except Exception:
                raw = row
        out.append(_normalize(raw, recipient))
    return out
