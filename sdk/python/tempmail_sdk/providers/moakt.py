"""
moakt.com：GET 语言首页与收件箱 HTML；凭证为 tm_session 等 Cookie（序列化在 token 内）；
列表解析 /{locale}/email/{uuid}，正文 GET .../html 解析 .email-body。
"""

import base64
import html
import json
import re
from typing import Dict, List, Optional, Tuple

import requests

from ..config import get_config
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "moakt"
ORIGIN = "https://www.moakt.com"
TOK_PREFIX = "mok1:"

_EMAIL_DIV_RE = re.compile(r'(?is)<div\s+id="email-address"\s*>([^<]+)</div>')
_HREF_EMAIL_RE = re.compile(
    r'href="(/[^"]+/email/[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})"'
)
_TITLE_RE = re.compile(r'(?is)<li\s+class="title"\s*>([^<]*)</li>')
_DATE_RE = re.compile(r'(?is)<li\s+class="date"[^>]*>[\s\S]*?<span[^>]*>([^<]+)</span>')
_SENDER_RE = re.compile(
    r'(?is)<li\s+class="sender"[^>]*>[\s\S]*?<span[^>]*>([\s\S]*?)</span>\s*</li>'
)
_BODY_RE = re.compile(r'(?is)<div\s+class="email-body"\s*>([\s\S]*?)</div>')
_FROM_ADDR_RE = re.compile(
    r"<([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})>"
)
_TAG_RE = re.compile(r"<[^>]+>")

_DEFAULT_HEADERS = {
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "Cache-Control": "no-cache",
    "DNT": "1",
    "Pragma": "no-cache",
    "Upgrade-Insecure-Requests": "1",
}


def _locale(domain: Optional[str]) -> str:
    s = (domain or "").strip()
    if not s or any(c in s for c in "/?#\\"):
        return "zh"
    return s


def _bare_get(url: str, headers: dict) -> requests.Response:
    c = get_config()
    kw: dict = {"timeout": c.timeout, "headers": headers, "verify": not c.insecure}
    if c.proxy:
        kw["proxies"] = {"http": c.proxy, "https": c.proxy}
    return requests.get(url, **kw)


def _parse_cookie_map(hdr: str) -> Dict[str, str]:
    m: Dict[str, str] = {}
    for part in hdr.split(";"):
        part = part.strip()
        if not part or "=" not in part:
            continue
        k, _, v = part.partition("=")
        k, v = k.strip(), v.strip()
        if k:
            m[k] = v
    return m


def _merge_cookie_hdr(prev: str, resp: requests.Response) -> str:
    d = _parse_cookie_map(prev)
    d.update(resp.cookies.get_dict())
    return "; ".join(f"{k}={d[k]}" for k in sorted(d.keys()))


def _page_headers(referer: str, ua: str) -> dict:
    return {
        **_DEFAULT_HEADERS,
        "User-Agent": ua,
        "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
        "Referer": referer,
    }


def _encode_sess(locale: str, cookie_hdr: str) -> str:
    raw = json.dumps({"l": locale, "c": cookie_hdr}, separators=(",", ":")).encode("utf-8")
    return TOK_PREFIX + base64.standard_b64encode(raw).decode("ascii")


def _decode_sess(tok: str) -> Tuple[str, str]:
    if not tok.startswith(TOK_PREFIX):
        raise ValueError("moakt: invalid session token")
    try:
        data = base64.standard_b64decode(tok[len(TOK_PREFIX) :].encode("ascii"))
        o = json.loads(data.decode("utf-8"))
    except (json.JSONDecodeError, ValueError) as e:
        raise ValueError("moakt: invalid session token") from e
    loc = (o.get("l") or "").strip()
    c = (o.get("c") or "").strip()
    if not loc or not c:
        raise ValueError("moakt: invalid session token")
    return loc, c


def _parse_inbox_email(html_s: str) -> str:
    m = _EMAIL_DIV_RE.search(html_s)
    if not m:
        raise RuntimeError("moakt: email-address not found")
    addr = html.unescape(m.group(1).strip())
    if not addr:
        raise RuntimeError("moakt: empty email-address")
    return addr


def _strip_tags(s: str) -> str:
    return _TAG_RE.sub(" ", s).strip()


def _list_mail_ids(html_s: str) -> List[str]:
    seen = set()
    out: List[str] = []
    for m in _HREF_EMAIL_RE.finditer(html_s):
        path = m.group(1)
        if "/delete" in path:
            continue
        mid = path.rsplit("/", 1)[-1]
        if len(mid) == 36 and mid not in seen:
            seen.add(mid)
            out.append(mid)
    return out


def _parse_detail(page: str, mid: str, recipient: str) -> dict:
    from_s = ""
    sm = _SENDER_RE.search(page)
    if sm:
        inner = html.unescape(sm.group(1))
        from_s = _strip_tags(inner)
        em = _FROM_ADDR_RE.search(inner)
        if em:
            from_s = em.group(1).strip()
    subj = ""
    tm = _TITLE_RE.search(page)
    if tm:
        subj = html.unescape(tm.group(1).strip())
    date_s = ""
    dm = _DATE_RE.search(page)
    if dm:
        date_s = html.unescape(dm.group(1).strip())
    body = ""
    bm = _BODY_RE.search(page)
    if bm:
        body = bm.group(1).strip()
    return {
        "id": mid,
        "to": recipient,
        "from": from_s,
        "subject": subj,
        "date": date_s,
        "html": body,
    }


def generate_email(domain: Optional[str] = None, **kwargs) -> EmailInfo:
    loc = _locale(domain)
    base = f"{ORIGIN}/{loc}"
    inbox = f"{base}/inbox"
    c = get_config()
    ua = (c.headers or {}).get("User-Agent") or (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    )

    r1 = _bare_get(base, _page_headers(base, ua))
    r1.raise_for_status()
    cookie_hdr = _merge_cookie_hdr("", r1)

    r2 = _bare_get(
        inbox,
        {**_page_headers(base, ua), "Cookie": cookie_hdr},
    )
    r2.raise_for_status()
    cookie_hdr = _merge_cookie_hdr(cookie_hdr, r2)
    html_s = r2.text

    email = _parse_inbox_email(html_s)
    if "tm_session" not in _parse_cookie_map(cookie_hdr):
        raise RuntimeError("moakt: missing tm_session cookie")
    tok = _encode_sess(loc, cookie_hdr)
    return EmailInfo(channel=CHANNEL, email=email, _token=tok)


def get_emails(email: str, token: str, **kwargs) -> List[Email]:
    loc, cookie_hdr = _decode_sess(token)
    inbox = f"{ORIGIN}/{loc}/inbox"
    c = get_config()
    ua = (c.headers or {}).get("User-Agent") or (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    )
    base_ref = f"{ORIGIN}/{loc}"

    r = _bare_get(
        inbox,
        {**_page_headers(base_ref, ua), "Cookie": cookie_hdr},
    )
    r.raise_for_status()
    ids = _list_mail_ids(r.text)
    out: List[Email] = []
    for mid in ids:
        detail = f"{ORIGIN}/{loc}/email/{mid}/html"
        refer = f"{ORIGIN}/{loc}/email/{mid}"
        rd = _bare_get(
            detail,
            {**_page_headers(refer, ua), "Cookie": cookie_hdr},
        )
        if rd.status_code != 200:
            continue
        raw = _parse_detail(rd.text, mid, email)
        out.append(normalize_email(raw, email))
    return out
