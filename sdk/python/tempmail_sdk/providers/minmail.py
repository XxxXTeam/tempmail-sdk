"""
MinMail 渠道
API: https://minmail.app/api
建箱 GET /mail/address 返回 visitorId、ck；收信 GET /mail/list 需请求头 visitor-id 与 ck。
"""

import json
import random
import string
import time
from typing import List, Optional, Tuple

from .. import http as tm_http
from ..logger import get_logger
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "minmail"
BASE = "https://minmail.app/api"

HEADERS_BASE = {
    "Accept": "*/*",
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "Origin": "https://minmail.app",
    "Referer": "https://minmail.app/cn",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
    "cache-control": "no-cache",
    "dnt": "1",
    "pragma": "no-cache",
    "priority": "u=1, i",
    "sec-ch-ua": '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
    "sec-ch-ua-mobile": "?0",
    "sec-ch-ua-platform": '"Windows"',
    "sec-fetch-dest": "empty",
    "sec-fetch-mode": "cors",
    "sec-fetch-site": "same-origin",
}


def _random_seg(n: int) -> str:
    chars = string.ascii_lowercase + string.digits
    return "".join(random.choice(chars) for _ in range(n))


def _visitor_id() -> str:
    return f"{_random_seg(8)}-{_random_seg(4)}-{_random_seg(4)}-{_random_seg(4)}-{_random_seg(12)}"


def _cookie_header() -> str:
    now = int(time.time() * 1000)
    rnd = random.randint(0, 999999)
    ga = f"GA1.1.{now}.{rnd}"
    return f"_ga={ga}; _ga_DFGB8WF1WG=GS2.1.s{now}$o1$g0$t{now}$j60$l0$h0"


def _headers_address_cookie(cookie: str) -> dict:
    h = dict(HEADERS_BASE)
    h["Cookie"] = cookie
    return h


def _headers_list(visitor_id: str, ck: str, cookie: str) -> dict:
    h = dict(HEADERS_BASE)
    h["visitor-id"] = visitor_id
    h["Cookie"] = cookie.strip() if cookie and cookie.strip() else _cookie_header()
    if ck:
        h["ck"] = ck
    return h


def _parse_token(token: Optional[str]) -> Tuple[str, str, str]:
    if not token or not str(token).strip():
        return "", "", ""
    t = str(token).strip()
    if t.startswith("{"):
        try:
            o = json.loads(t)
            vid = o.get("visitorId") or o.get("visitor_id") or ""
            return str(vid), str(o.get("ck") or ""), str(o.get("cookie") or "")
        except (json.JSONDecodeError, TypeError):
            get_logger().debug("minmail: token JSON parse failed", exc_info=True)
    return t, "", ""


def _to_matches(header: str, want: str) -> bool:
    w = (want or "").strip().lower()
    if not w:
        return True
    h = (header or "").strip().lower()
    if not h:
        return True
    if h == w:
        return True
    if "<" in h and ">" in h:
        start, end = h.find("<"), h.find(">")
        if start >= 0 and end > start:
            inner = h[start + 1 : end].strip()
            if inner == w:
                return True
    return w in h


def generate_email() -> EmailInfo:
    cook = _cookie_header()
    resp = tm_http.get(
        f"{BASE}/mail/address?refresh=true&expire=1440&part=main",
        headers=_headers_address_cookie(cook),
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    address = data.get("address")
    if not address:
        raise RuntimeError("minmail: empty address")
    vid = data.get("visitorId") or data.get("visitor_id") or ""
    if not vid:
        vid = _visitor_id()
    ck = data.get("ck") or ""
    stored = json.dumps(
        {"visitorId": vid, "ck": ck, "cookie": cook},
        separators=(",", ":"),
    )
    expire_min = int(data.get("expire") or 0)
    expires_at = None
    if expire_min > 0:
        expires_at = int(time.time() * 1000) + expire_min * 60 * 1000
    return EmailInfo(channel=CHANNEL, email=address, _token=stored, expires_at=expires_at)


def get_emails(email: str, token: Optional[str] = None) -> List[Email]:
    vid, ck, cook = _parse_token(token)
    if not vid:
        vid = _visitor_id()
    resp = tm_http.get(
        f"{BASE}/mail/list?part=main",
        headers=_headers_list(vid, ck, cook),
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    messages = data.get("message") or []
    if not isinstance(messages, list):
        return []
    out: List[Email] = []
    for raw in messages:
        if not isinstance(raw, dict):
            continue
        if raw.get("to") and not _to_matches(str(raw.get("to")), email):
            continue
        flat = {
            "id": raw.get("id"),
            "from": raw.get("from"),
            "to": raw.get("to") or email,
            "subject": raw.get("subject"),
            "text": raw.get("preview"),
            "html": raw.get("content"),
            "date": raw.get("date"),
            "isRead": raw.get("isRead"),
        }
        out.append(normalize_email(flat, email))
    return out
