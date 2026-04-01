"""
Maildrop — https://maildrop.cx
GET /api/suffixes.php、GET /api/emails.php
"""

import random
import string
from urllib.parse import urlencode

from .. import http as tm_http
from ..types import EmailInfo, Email

CHANNEL = "maildrop"
BASE = "https://maildrop.cx"
EXCLUDED_SUFFIX = "transformer.edu.kg"

_HEADERS = {
    "Accept": "application/json, text/plain, */*",
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "Cache-Control": "no-cache",
    "DNT": "1",
    "Pragma": "no-cache",
    "Referer": "https://maildrop.cx/zh-cn/app",
    "sec-ch-ua": '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
    "sec-ch-ua-mobile": "?0",
    "sec-ch-ua-platform": '"Windows"',
    "sec-fetch-dest": "empty",
    "sec-fetch-mode": "cors",
    "sec-fetch-site": "same-origin",
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    "x-requested-with": "XMLHttpRequest",
}


def _random_local(length: int = 10) -> str:
    return "".join(random.choices(string.ascii_lowercase + string.digits, k=length))


def _fetch_suffixes() -> list:
    r = tm_http.get(f"{BASE}/api/suffixes.php", headers=_HEADERS)
    r.raise_for_status()
    data = r.json()
    if not isinstance(data, list):
        raise ValueError("maildrop: invalid suffixes response")
    ex = EXCLUDED_SUFFIX.lower()
    out = []
    for x in data:
        if isinstance(x, str):
            t = x.strip()
            if t and t.lower() != ex:
                out.append(t)
    if not out:
        raise ValueError("maildrop: no domains available")
    return out


def _pick_suffix(suffixes: list, preferred: str | None) -> str:
    if preferred and str(preferred).strip():
        p = str(preferred).strip().lower()
        for d in suffixes:
            if d.lower() == p:
                return d
    return random.choice(suffixes)


def _cx_date_to_iso(s: str) -> str:
    s = (s or "").strip()
    if len(s) == 19 and s[10] == " ":
        return s[:10] + "T" + s[11:] + "Z"
    return s


def generate_email(domain: str | None = None, **kwargs) -> EmailInfo:
    """随机本地部分；域名来自 suffixes，排除 transformer.edu.kg。"""
    suffixes = _fetch_suffixes()
    dom = _pick_suffix(suffixes, domain)
    local = _random_local()
    email = f"{local}@{dom}"
    return EmailInfo(channel=CHANNEL, email=email, _token=email)


def get_emails(token: str, email: str = "", **kwargs) -> list:
    addr = (email or "").strip() or (token or "").strip()
    if not addr:
        raise ValueError("maildrop: empty address")
    qs = urlencode({"addr": addr, "page": "1", "limit": "20"})
    r = tm_http.get(f"{BASE}/api/emails.php?{qs}", headers=_HEADERS)
    r.raise_for_status()
    data = r.json() if r.content else {}
    rows = data.get("emails") or []
    if not isinstance(rows, list):
        return []
    out = []
    for row in rows:
        if not isinstance(row, dict):
            continue
        desc = (row.get("description") or "").strip()
        ir = row.get("isRead")
        is_read = ir is True or ir == 1
        out.append(
            Email(
                id=str(row.get("id", "")),
                from_addr=(row.get("from_addr") or "").strip(),
                to=addr,
                subject=(row.get("subject") or "").strip(),
                text=desc,
                html="",
                date=_cx_date_to_iso(row.get("date") or ""),
                is_read=is_read,
                attachments=[],
            )
        )
    return out
