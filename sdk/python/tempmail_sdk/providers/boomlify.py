"""
Boomlify 渠道
API: https://v1.boomlify.com
"""

import random
import string
from typing import List
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "boomlify"
BASE = "https://v1.boomlify.com"

HEADERS = {
    "Accept": "application/json, text/plain, */*",
    "Accept-Language": "zh",
    "Origin": "https://boomlify.com",
    "Referer": "https://boomlify.com/",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
    "sec-ch-ua": '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
    "sec-ch-ua-mobile": "?0",
    "sec-ch-ua-platform": '"Windows"',
    "sec-fetch-dest": "empty",
    "sec-fetch-mode": "cors",
    "sec-fetch-site": "same-site",
    "x-user-language": "zh",
}


def _random_local(n: int = 8) -> str:
    chars = string.ascii_lowercase + string.digits
    return "".join(random.choice(chars) for _ in range(n))


def _get_domains() -> list:
    resp = tm_http.get(f"{BASE}/domains/public", headers=HEADERS, timeout=15)
    resp.raise_for_status()
    items = resp.json()
    if not isinstance(items, list):
        return []
    out = []
    for d in items:
        if not isinstance(d, dict):
            continue
        dom = d.get("domain")
        if not dom:
            continue
        active = d.get("is_active")
        if active is None:
            active = d.get("isActive")
        if active == 1 or active is True:
            out.append(dom)
    return out


def generate_email() -> EmailInfo:
    domains = _get_domains()
    if not domains:
        raise RuntimeError("boomlify: no domains")
    domain = random.choice(domains)
    local = _random_local(8)
    return EmailInfo(channel=CHANNEL, email=f"{local}@{domain}")


def get_emails(email: str) -> List[Email]:
    resp = tm_http.get(
        f"{BASE}/emails/public/{quote(email, safe='')}",
        headers=HEADERS,
        timeout=15,
    )
    resp.raise_for_status()
    rows = resp.json()
    if not isinstance(rows, list):
        return []
    emails: List[Email] = []
    for raw in rows:
        if not isinstance(raw, dict):
            continue
        from_addr = raw.get("from_email") or raw.get("from_name") or ""
        flat = {
            "id": raw.get("id"),
            "from": from_addr,
            "to": email,
            "subject": raw.get("subject"),
            "text": raw.get("body_text"),
            "html": raw.get("body_html"),
            "received_at": raw.get("received_at"),
            "isRead": False,
        }
        emails.append(normalize_email(flat, email))
    return emails
