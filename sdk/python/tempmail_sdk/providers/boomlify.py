"""
Boomlify 渠道
API: https://v1.boomlify.com
"""

import random
import re
from typing import List

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

_RE_UUID_LOCAL = re.compile(
    r"^[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$",
    re.I,
)


def _get_domains() -> List[dict]:
    resp = tm_http.get(f"{BASE}/domains/public", headers=HEADERS, timeout=15)
    resp.raise_for_status()
    items = resp.json()
    if not isinstance(items, list):
        return []
    out: List[dict] = []
    for d in items:
        if not isinstance(d, dict):
            continue
        dom = d.get("domain")
        did = d.get("id")
        if not dom or not did:
            continue
        active = d.get("is_active")
        if active is None:
            active = d.get("isActive")
        if active == 1 or active is True:
            out.append({"id": str(did), "domain": str(dom)})
    return out


def _create_public_inbox(domain_id: str) -> str:
    h = dict(HEADERS)
    h["Content-Type"] = "application/json"
    resp = tm_http.post(
        f"{BASE}/emails/public/create",
        headers=h,
        json={"domainId": domain_id},
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, dict):
        raise RuntimeError("boomlify: invalid create response")
    if data.get("error"):
        msg = str(data.get("error"))
        if data.get("message"):
            msg = f"{msg} — {data.get('message')}"
        if data.get("captchaRequired"):
            msg = f"{msg}（服务端限流/需验证码，请稍后重试）"
        raise RuntimeError(f"boomlify: {msg}")
    box_id = data.get("id")
    if not box_id:
        raise RuntimeError("boomlify: public create returned no inbox id")
    return str(box_id)


def _inbox_path_segment(email: str) -> str:
    local = email.rsplit("@", 1)[0] if "@" in email else email
    if _RE_UUID_LOCAL.match((local or "").strip()):
        return local.strip()
    return email


def generate_email() -> EmailInfo:
    domains = _get_domains()
    if not domains:
        raise RuntimeError("boomlify: no domains")
    pick = random.choice(domains)
    box_id = _create_public_inbox(pick["id"])
    addr = f"{box_id}@{pick['domain']}"
    return EmailInfo(channel=CHANNEL, email=addr, _token=box_id)


def get_emails(email: str) -> List[Email]:
    from urllib.parse import quote

    seg = _inbox_path_segment(email)
    resp = tm_http.get(
        f"{BASE}/emails/public/{quote(seg, safe='')}",
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
