"""
Fake Legal 渠道 — https://imgui.de
"""

import random
import string
from typing import List, Optional

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "fake-legal"
BASE = "https://imgui.de"

HEADERS = {
    "Accept": "application/json, text/plain, */*",
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
    "Cache-Control": "no-cache",
    "DNT": "1",
    "Pragma": "no-cache",
    "Referer": "https://imgui.de/",
    "sec-ch-ua": '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
    "sec-ch-ua-mobile": "?0",
    "sec-ch-ua-platform": '"Windows"',
    "sec-fetch-dest": "empty",
    "sec-fetch-mode": "cors",
    "sec-fetch-site": "same-origin",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
}


def _fetch_domains() -> List[str]:
    resp = tm_http.get(f"{BASE}/api/domains", headers=HEADERS, timeout=15)
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, dict):
        return []
    raw = data.get("domains")
    if not isinstance(raw, list):
        return []
    out: List[str] = []
    for d in raw:
        if isinstance(d, str) and d.strip():
            out.append(d.strip())
    return out


def _pick_domain(domains: List[str], preferred: Optional[str]) -> str:
    if preferred and preferred.strip():
        p = preferred.strip().lower()
        for d in domains:
            if d.lower() == p:
                return d
        raise ValueError(f"fake-legal: domain not available: {p}")
    return random.choice(domains)


def _random_username(length: int) -> str:
    chars = string.ascii_letters + string.digits
    return "".join(random.choice(chars) for _ in range(length))


def generate_email(domain: Optional[str] = None, channel: str = CHANNEL) -> EmailInfo:
    domains = _fetch_domains()
    if not domains:
        raise RuntimeError("fake-legal: no domains")
    d = _pick_domain(domains, domain)
    from urllib.parse import quote

    if d == "imgui.de" or d == "pulsewebmenu.de":
        # imgui-de / pulsewebmenu-de 用 POST 保根域
        url = f"{BASE}/api/inbox/custom"
        body = {
            "username": _random_username(12),
            "domain": d,
        }
        resp = tm_http.post(url, json=body, headers=HEADERS, timeout=15)
    else:
        # fake-legal 用 GET 创建
        url = f"{BASE}/api/inbox/new?domain={quote(d, safe='')}"
        resp = tm_http.get(url, headers=HEADERS, timeout=15)

    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, dict) or not data.get("success"):
        raise RuntimeError("fake-legal: create inbox failed")
    addr = data.get("address")
    if not addr or not str(addr).strip():
        raise RuntimeError("fake-legal: missing address")
    return EmailInfo(channel=channel, email=str(addr).strip())


def get_emails(email: str) -> List[Email]:
    from urllib.parse import quote

    if not (email or "").strip():
        raise ValueError("fake-legal: empty email")
    e = email.strip()
    resp = tm_http.get(
        f"{BASE}/api/inbox/{quote(e, safe='')}",
        headers=HEADERS,
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, dict) or not data.get("success"):
        return []
    rows = data.get("emails")
    if not isinstance(rows, list):
        return []
    emails: List[Email] = []
    for raw in rows:
        if not isinstance(raw, dict):
            continue
        emails.append(normalize_email(raw, e))
    return emails
