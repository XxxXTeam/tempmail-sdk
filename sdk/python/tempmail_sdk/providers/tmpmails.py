"""
tmpmails.com：GET 首页下发 user_sign Cookie 与页面中的临时地址；
收信为 Next.js Server Action POST（Accept: text/x-component），body 为 [user_sign, email, 0]。
"""

import json
import re
from typing import List, Optional

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import EmailInfo, Email

CHANNEL = "tmpmails"
ORIGIN = "https://tmpmails.com"
TOKEN_SEP = "\t"

_EMAIL_RE = re.compile(r"[a-zA-Z0-9._-]+@tmpmails\.com")
_PAGE_CHUNK_RE = re.compile(r"/_next/static/chunks/app/%5Blocale%5D/page-[a-f0-9]+\.js")
_INBOX_ACTION_RE = re.compile(
    r'"([0-9a-f]+)",[^,]+,void 0,[^,]+,"getInboxList"'
)

_DEFAULT_HEADERS = {
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "Cache-Control": "no-cache",
    "DNT": "1",
    "Pragma": "no-cache",
}


def _locale(domain: Optional[str]) -> str:
    s = (domain or "").strip()
    return s or "zh"


def _user_sign_from_response(resp) -> str:
    for c in resp.cookies.items():
        if str(c[0]).lower() == "user_sign":
            v = str(c[1]).strip()
            if v:
                return v
    raise RuntimeError("tmpmails: missing user_sign cookie")


def _pick_email(html: str) -> str:
    support = "support@tmpmails.com"
    counts: dict[str, int] = {}
    for m in _EMAIL_RE.finditer(html):
        addr = m.group(0)
        if addr.lower() == support:
            continue
        counts[addr] = counts.get(addr, 0) + 1
    if not counts:
        raise RuntimeError("tmpmails: no inbox address in page")
    return sorted(counts.items(), key=lambda kv: (-kv[1], kv[0]))[0][0]


def _fetch_inbox_action_id(html: str) -> str:
    m = _PAGE_CHUNK_RE.search(html)
    if not m:
        raise RuntimeError("tmpmails: page chunk script not found")
    sub = m.group(0)
    url = ORIGIN + sub
    r = tm_http.get(
        url,
        headers={
            **_DEFAULT_HEADERS,
            "Accept": "*/*",
            "Referer": ORIGIN + "/",
        },
    )
    r.raise_for_status()
    m2 = _INBOX_ACTION_RE.search(r.text)
    if not m2:
        raise RuntimeError("tmpmails: getInboxList action not found in chunk")
    return m2.group(1)


def generate_email(domain: Optional[str] = None, **kwargs) -> EmailInfo:
    loc = _locale(domain)
    page_url = f"{ORIGIN}/{loc}"
    resp = tm_http.get(
        page_url,
        headers={
            **_DEFAULT_HEADERS,
            "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            "Referer": page_url,
            "Upgrade-Insecure-Requests": "1",
        },
    )
    resp.raise_for_status()
    html = resp.text
    user_sign = _user_sign_from_response(resp)
    email = _pick_email(html)
    action_id = _fetch_inbox_action_id(html)
    token = TOKEN_SEP.join((loc, user_sign, action_id))
    return EmailInfo(channel=CHANNEL, email=email, _token=token)


def _parse_inbox_response(recipient: str, raw: str) -> List[Email]:
    text = raw.replace('"$undefined"', "null")
    data_err = None
    for line in text.split("\n"):
        line = line.strip()
        if not line or ":" not in line:
            continue
        _, rest = line.split(":", 1)
        json_part = rest.strip()
        if not json_part.startswith("{"):
            continue
        try:
            wrap = json.loads(json_part)
        except json.JSONDecodeError:
            continue
        if wrap.get("code") != 200:
            continue
        data = wrap.get("data") or {}
        raw_list = data.get("list")
        if not isinstance(raw_list, list):
            data_err = RuntimeError("tmpmails: invalid list")
            continue
        out = []
        for item in raw_list:
            if isinstance(item, dict):
                out.append(normalize_email(item, recipient))
        return out
    if data_err:
        raise data_err
    raise RuntimeError("tmpmails: inbox payload not found")


def get_emails(email: str, token: str, **kwargs) -> List[Email]:
    parts = token.split(TOKEN_SEP)
    if len(parts) != 3:
        raise ValueError("tmpmails: invalid session token")
    locale, user_sign, action_id = parts
    post_url = f"{ORIGIN}/{locale}"
    body = json.dumps([user_sign, email, 0], separators=(",", ":"))
    resp = tm_http.post(
        post_url,
        headers={
            **_DEFAULT_HEADERS,
            "Accept": "text/x-component",
            "Content-Type": "text/plain;charset=UTF-8",
            "Next-Action": action_id,
            "Origin": ORIGIN,
            "Referer": post_url,
            "Cookie": f"NEXT_LOCALE={locale}; user_sign={user_sign}",
        },
        data=body.encode("utf-8"),
    )
    resp.raise_for_status()
    return _parse_inbox_response(email, resp.text)
