"""
10minutemail.one：SSR __NUXT_DATA__ 中的 mailServiceToken（JWT）+ 页面 emailDomains；
收信 GET web.10minutemail.one/api/v1/mailbox/{email}
"""

import base64
import json
import re
import secrets
import time
from typing import Any, Dict, List, Optional

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import EmailInfo, Email

CHANNEL = "10minute-one"
SITE_ORIGIN = "https://10minutemail.one"
API_BASE = "https://web.10minutemail.one/api/v1"
_DEFAULT_UA = (
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
)

_NUXT_DATA_RE = re.compile(
    r'<script[^>]*\bid="__NUXT_DATA__"[^>]*>([\s\S]*?)</script>',
    re.I,
)
_JWT_RE = re.compile(r"^[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+$")


def _enc_mailbox_email(email: str) -> str:
    if "@" in email:
        local, _, dom = email.partition("@")
        from urllib.parse import quote

        return f"{quote(local, safe='')}%40{quote(dom, safe='')}"
    from urllib.parse import quote

    return quote(email, safe="")


def _parse_nuxt_array(html: str) -> List[Any]:
    m = _NUXT_DATA_RE.search(html)
    if not m:
        raise RuntimeError("10minute-one: __NUXT_DATA__ not found")
    return json.loads(m.group(1).strip())


def _resolve_ref(arr: List[Any], value: Any, depth: int = 0) -> Any:
    if depth > 64:
        return value
    if isinstance(value, bool):
        return value
    if isinstance(value, int) and not isinstance(value, bool) and value >= 0:
        if value < len(arr):
            return _resolve_ref(arr, arr[value], depth + 1)
    return value


def _parse_mail_service_token(arr: List[Any]) -> str:
    n = min(len(arr), 48)
    for i in range(n):
        el = arr[i]
        if isinstance(el, dict) and "mailServiceToken" in el:
            t = _resolve_ref(arr, el["mailServiceToken"])
            if isinstance(t, str) and _JWT_RE.match(t):
                return t
    for el in arr:
        if isinstance(el, dict) and "mailServiceToken" in el:
            t = _resolve_ref(arr, el["mailServiceToken"])
            if isinstance(t, str) and _JWT_RE.match(t):
                return t
    for el in arr:
        if isinstance(el, str) and _JWT_RE.match(el):
            return el
    raise RuntimeError("10minute-one: mailServiceToken not found")


def _parse_quoted_json_array(html: str, field: str) -> List[str]:
    key = f'{field}:"'
    start = html.find(key)
    if start < 0:
        return []
    slice_start = start + len(key)
    if slice_start >= len(html) or html[slice_start] != "[":
        return []
    depth = 0
    j = slice_start
    while j < len(html):
        c = html[j]
        if c == "[":
            depth += 1
        elif c == "]":
            depth -= 1
            if depth == 0:
                j += 1
                break
        j += 1
    raw = html[slice_start:j]
    unesc = raw.replace('\\"', '"')
    try:
        v = json.loads(unesc)
        return v if isinstance(v, list) else []
    except json.JSONDecodeError:
        return []


def _pick_locale(domain: Optional[str]) -> str:
    s = (domain or "").strip()
    if not s:
        return "zh"
    if "." in s and "/" not in s:
        return "zh"
    return s


def _pick_mailbox_domain(hint: Optional[str], available: List[str]) -> str:
    if not available:
        raise RuntimeError("10minute-one: no email domains")
    if hint:
        h = hint.strip().lower()
        if "." in h:
            for d in available:
                if d.lower() == h:
                    return d
    return secrets.choice(available)


def _random_local(n: int) -> str:
    alphabet = "abcdefghijklmnopqrstuvwxyz0123456789"
    return "".join(secrets.choice(alphabet) for _ in range(n))


def _jwt_exp_ms(token: str) -> Optional[int]:
    parts = token.split(".")
    if len(parts) < 2:
        return None
    b = parts[1] + "=" * (-len(parts[1]) % 4)
    try:
        pad = b.replace("-", "+").replace("_", "/")
        payload = json.loads(base64.b64decode(pad).decode("utf-8"))
        exp = payload.get("exp")
        if isinstance(exp, (int, float)):
            return int(exp * 1000)
    except (ValueError, json.JSONDecodeError, OSError):
        pass
    return None


def _api_headers(tok: str) -> Dict[str, str]:
    return {
        "Accept": "*/*",
        "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
        "Authorization": f"Bearer {tok}",
        "Cache-Control": "no-cache",
        "Content-Type": "application/json",
        "DNT": "1",
        "Origin": SITE_ORIGIN,
        "Pragma": "no-cache",
        "Referer": f"{SITE_ORIGIN}/",
        "User-Agent": _DEFAULT_UA,
        "X-Request-ID": secrets.token_hex(16),
        "X-Timestamp": str(int(time.time())),
    }


def _item_needs_detail(m: Dict[str, Any]) -> bool:
    if not str(m.get("id", "")).strip():
        return False
    subj = str(m.get("subject") or m.get("mail_title") or "").strip()
    body = (
        str(m.get("text") or m.get("body") or m.get("html") or m.get("mail_text") or "")
        .strip()
    )
    return not subj and not body


def generate_email(domain: Optional[str] = None, **kwargs) -> EmailInfo:
    loc = _pick_locale(domain)
    page_url = f"{SITE_ORIGIN}/{loc}"
    r = tm_http.get(
        page_url,
        headers={
            "User-Agent": _DEFAULT_UA,
            "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
            "Cache-Control": "no-cache",
            "Pragma": "no-cache",
            "DNT": "1",
            "Referer": page_url,
        },
    )
    r.raise_for_status()
    html = r.text
    arr = _parse_nuxt_array(html)
    token = _parse_mail_service_token(arr)

    domains = _parse_quoted_json_array(html, "emailDomains")
    if not domains:
        domains = ["xghff.com", "oqqaj.com", "psovv.com"]

    blocked = {u.lower() for u in _parse_quoted_json_array(html, "blockedUsernames")}

    dom_hint = domain.strip() if domain and "." in domain.strip() else None
    mail_domain = _pick_mailbox_domain(dom_hint, domains)

    local = ""
    for _ in range(32):
        cand = _random_local(10)
        if cand.lower() not in blocked:
            local = cand
            break
    if not local:
        raise RuntimeError("10minute-one: could not pick username")

    address = f"{local}@{mail_domain}"
    exp_ms = _jwt_exp_ms(token)
    return EmailInfo(
        channel=CHANNEL,
        email=address,
        _token=token,
        expires_at=exp_ms,
    )


def get_emails(email: str, token: str) -> List[Email]:
    url = f"{API_BASE}/mailbox/{_enc_mailbox_email(email)}"
    r = tm_http.get(url, headers=_api_headers(token))
    r.raise_for_status()
    data = r.json()
    if not isinstance(data, list):
        raise RuntimeError("10minute-one: invalid inbox JSON")

    out: List[Email] = []
    for raw in data:
        if not isinstance(raw, dict):
            continue
        row: Dict[str, Any] = dict(raw)
        if _item_needs_detail(row):
            mid = str(row.get("id", ""))
            from urllib.parse import quote

            du = f"{API_BASE}/mailbox/{_enc_mailbox_email(email)}/{quote(mid, safe='')}"
            try:
                d = tm_http.get(du, headers=_api_headers(token))
                if d.ok:
                    detail = d.json()
                    if isinstance(detail, dict):
                        for k, v in detail.items():
                            row.setdefault(k, v)
            except OSError:
                pass
        out.append(normalize_email(row, email))
    return out
