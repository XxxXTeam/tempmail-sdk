"""
moakt.com：GET 语言首页与收件箱 HTML；凭证为 tm_session 等 Cookie（序列化在 token 内）；
列表解析 /{locale}/email/{uuid}，正文 GET .../html 解析 .email-body。
"""

import base64
import html
import json
import re
import secrets
from typing import Dict, List, Optional, Tuple

import requests

from ..config import get_config
from .. import http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "moakt"
ORIGIN = "https://www.moakt.com"
TOK_PREFIX = "mok1:"

_EMAIL_DIV_RE = re.compile(r'(?is)<div\s+id="email-address"\s*>([^<]+)</div>')
_DOMAIN_OPTION_RE = re.compile(r'(?is)<option\s+value="([^"]+)">\s*@[^<]+</option>')
_MAIL_DOMAIN_RE = re.compile(
    r"(?i)^[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?(?:\.[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?)+$"
)
_HREF_EMAIL_RE = re.compile(
    r'href="(/[^"]+/email/[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})"'
)
_TITLE_RE = re.compile(r'(?is)<li\s+class="title"\s*>([^<]*)</li>')
_DATE_RE = re.compile(r'(?is)<li\s+class="date"[^>]*>[\s\S]*?<span[^>]*>([^<]+)</span>')
_SENDER_RE = re.compile(
    r'(?is)<li\s+class="sender"[^>]*>[\s\S]*?<span[^>]*>([\s\S]*?)</span>\s*</li>'
)
_BODY_OPEN_RE = re.compile(r'(?is)<div\s+class="email-body"\s*>')


def _extract_body_html(page: str) -> str:
    """使用栈式深度匹配提取 email-body div 的完整内部 HTML，
    避免非贪婪正则在嵌套 div 时截断正文。
    """
    m = _BODY_OPEN_RE.search(page)
    if not m:
        return ""
    start = m.end()
    pos = start
    depth = 1
    while pos < len(page) and depth > 0:
        next_open = page.find("<div", pos)
        next_close = page.find("</div>", pos)
        if next_close == -1:
            break
        if next_open != -1 and next_open < next_close:
            depth += 1
            pos = next_open + 4
        else:
            depth -= 1
            if depth == 0:
                return page[start:next_close].strip()
            pos = next_close + 6
    return ""
_FROM_ADDR_RE = re.compile(r"<([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})>")
_TAG_RE = re.compile(r"<[^>]+>")

_DEFAULT_HEADERS = {
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "Cache-Control": "no-cache",
    "DNT": "1",
    "Pragma": "no-cache",
    "Upgrade-Insecure-Requests": "1",
}


def _request_parts(domain: Optional[str]) -> Tuple[str, str]:
    s = (domain or "").strip()
    if not s or any(c in s for c in "/?#\\"):
        return "zh", ""
    if _MAIL_DOMAIN_RE.match(s):
        return "zh", s.lstrip("@").lower()
    return s, ""


def _parse_server_domains(page: str) -> set[str]:
    return {
        m.group(1).strip().lstrip("@").lower()
        for m in _DOMAIN_OPTION_RE.finditer(page)
        if m.group(1).strip()
    }


def _random_local(length: int = 12) -> str:
    chars = "abcdefghijklmnopqrstuvwxyz0123456789"
    return "".join(secrets.choice(chars) for _ in range(length))


def _email_domain(email: str) -> str:
    _, sep, domain = email.rpartition("@")
    return domain.strip().lower() if sep else ""


def _bare_get(url: str, headers: dict, **kwargs) -> requests.Response:
    """使用共享HTTP客户端发起GET请求（复用连接池）"""
    return http.get(url, headers=headers, **kwargs)


def _bare_post(url: str, headers: dict, **kwargs) -> requests.Response:
    """使用共享HTTP客户端发起POST请求（复用连接池）"""
    return http.post(url, headers=headers, **kwargs)


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
    raw = json.dumps({"l": locale, "c": cookie_hdr}, separators=(",", ":")).encode(
        "utf-8"
    )
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
    body = _extract_body_html(page)
    return {
        "id": mid,
        "to": recipient,
        "from": from_s,
        "subject": subj,
        "date": date_s,
        "html": body,
    }


def generate_email(domain: Optional[str] = None, **kwargs) -> EmailInfo:
    loc, mail_domain = _request_parts(domain)
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

    if mail_domain:
        if mail_domain not in _parse_server_domains(r1.text):
            raise RuntimeError(f"moakt: unsupported domain {mail_domain}")
        post_data: dict = {
            "setemail": "",
            "username": _random_local(),
            "domain": mail_domain,
            "preferred_domain": "",
        }
    else:
        post_data = {"random": "1"}

    # POST /inbox 创建邮箱，捕获 302 中的 tm_session cookie
    r2 = _bare_post(
        inbox,
        {
            **_page_headers(base, ua),
            "Content-Type": "application/x-www-form-urlencoded",
            "Cookie": cookie_hdr,
        },
        data=post_data,
        allow_redirects=False,
    )
    cookie_hdr = _merge_cookie_hdr(cookie_hdr, r2)

    if "tm_session" not in _parse_cookie_map(cookie_hdr):
        raise RuntimeError("moakt: missing tm_session cookie")

    # GET /inbox 获取邮箱地址
    r3 = _bare_get(
        inbox,
        {**_page_headers(base, ua), "Cookie": cookie_hdr},
    )
    r3.raise_for_status()
    cookie_hdr = _merge_cookie_hdr(cookie_hdr, r3)
    html_s = r3.text

    email = _parse_inbox_email(html_s)
    if mail_domain and _email_domain(email) != mail_domain:
        raise RuntimeError(
            f"moakt: domain mismatch expected={mail_domain} actual={_email_domain(email)}"
        )
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
