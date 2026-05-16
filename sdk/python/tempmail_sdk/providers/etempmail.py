"""
eTempMail — https://etempmail.com
先 GET /zh 建立 ci_session，再 POST /getEmailAddress、/getInbox。
"""

from typing import List

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "etempmail"
BASE = "https://etempmail.com"

HEADERS = {
    "Accept": "*/*",
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "Cache-Control": "no-cache",
    "DNT": "1",
    "Origin": BASE,
    "Pragma": "no-cache",
    "Referer": f"{BASE}/zh",
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
    "X-Requested-With": "XMLHttpRequest",
}


def _cookie_header_from_response(r) -> str:
    parts = []
    for c in r.cookies:
        parts.append(f"{c.name}={c.value}")
    return "; ".join(parts)


def _merge_cookie_header(existing: str, r) -> str:
    m = {}
    for seg in existing.split(";"):
        seg = seg.strip()
        if not seg or "=" not in seg:
            continue
        k, _, v = seg.partition("=")
        k, v = k.strip(), v.strip()
        if k:
            m[k] = v
    for c in r.cookies:
        m[c.name] = c.value
    return "; ".join(f"{k}={v}" for k, v in m.items())


def generate_email() -> EmailInfo:
    r = tm_http.get(f"{BASE}/zh", headers=HEADERS, timeout=15)
    r.raise_for_status()
    cookie = _cookie_header_from_response(r)
    if "ci_session=" not in cookie:
        raise RuntimeError("etempmail: no ci_session cookie")
    h2 = dict(HEADERS)
    h2["Cookie"] = cookie
    r2 = tm_http.post(f"{BASE}/getEmailAddress", headers=h2, data=b"", timeout=15)
    r2.raise_for_status()
    cookie = _merge_cookie_header(cookie, r2)
    data = r2.json()
    if not isinstance(data, dict):
        raise RuntimeError("etempmail: invalid create response")
    addr = data.get("address")
    if not addr:
        raise RuntimeError("etempmail: no address in response")
    created = None
    ct = data.get("creation_time")
    if ct is not None:
        try:
            sec = int(str(ct))
            if sec > 0:
                from datetime import datetime, timezone
                created = datetime.fromtimestamp(sec, tz=timezone.utc).isoformat()
        except (ValueError, OSError):
            pass
    return EmailInfo(channel=CHANNEL, email=str(addr), _token=cookie, created_at=created)


def get_emails(email: str, cookie: str) -> List[Email]:
    if not cookie:
        raise ValueError("token is required for etempmail channel")
    h = dict(HEADERS)
    h["Cookie"] = cookie
    r = tm_http.post(f"{BASE}/getInbox", headers=h, data=b"", timeout=15)
    r.raise_for_status()
    rows = r.json()
    if not isinstance(rows, list):
        return []
    out: List[Email] = []
    for i, raw in enumerate(rows):
        if not isinstance(raw, dict):
            continue
        from_addr = raw.get("from") or ""
        subj = raw.get("subject") or ""
        dt = raw.get("date") or ""
        body = raw.get("body") or ""
        flat = {
            "id": f"{from_addr}\n{subj}\n{dt}\n{i}\n{email}",
            "from": from_addr,
            "subject": subj,
            "body": body,
            "date": dt,
            "isRead": False,
        }
        out.append(normalize_email(flat, email))
    return out
