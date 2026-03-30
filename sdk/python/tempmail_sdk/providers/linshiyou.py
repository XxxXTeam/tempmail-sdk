"""
linshiyou.com 临时邮：NEXUS_TOKEN + tmail-emails Cookie，邮件接口返回 HTML 分片
"""

import html
import re
from .. import http as tm_http
from ..types import Email, EmailAttachment, EmailInfo

CHANNEL = "linshiyou"
ORIGIN = "https://linshiyou.com"

DEFAULT_HEADERS = {
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
    "accept-language": "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "cache-control": "no-cache",
    "dnt": "1",
    "pragma": "no-cache",
    "priority": "u=1, i",
    "referer": f"{ORIGIN}/",
    "sec-ch-ua": '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
    "sec-ch-ua-mobile": "?0",
    "sec-ch-ua-platform": '"Windows"',
    "sec-fetch-dest": "empty",
    "sec-fetch-mode": "cors",
    "sec-fetch-site": "same-origin",
}

_RE_LIST_ID = re.compile(r'id="tmail-email-list-([a-f0-9]+)"', re.I)
_RE_DIV = re.compile(r'<div class="([^"]+)"[^>]*>([\s\S]*?)</div>', re.I)
_RE_SRCDOC = re.compile(r'\ssrcdoc="([^"]*)"', re.I)
_RE_DOWNLOAD = re.compile(r'href="(/api/download\?id=[^"]+)"', re.I)
_RE_TAGS = re.compile(r"<[^>]+>")


def _strip_tags(s: str) -> str:
    t = _RE_TAGS.sub(" ", s)
    return " ".join(html.unescape(t).split())


def _pick_div(fragment: str, class_name: str) -> str:
    for m in _RE_DIV.finditer(fragment):
        if m.group(1) == class_name:
            return _strip_tags(m.group(2)).strip()
    return ""


def _parse_date(s: str) -> str:
    from datetime import datetime, timezone

    s = (s or "").strip()
    if not s:
        return ""
    for fmt in ("%Y-%m-%d %H:%M:%S", "%Y-%m-%dT%H:%M:%S"):
        try:
            dt = datetime.strptime(s, fmt)
            return dt.replace(tzinfo=timezone.utc).isoformat().replace("+00:00", "Z")
        except ValueError:
            continue
    return ""


def _extract_srcdoc(body_part: str) -> str:
    m = _RE_SRCDOC.search(body_part)
    if not m:
        return ""
    return html.unescape(m.group(1))


def _parse_segments(raw: str, recipient: str) -> list:
    raw = (raw or "").strip()
    if not raw:
        return []
    out = []
    for seg in raw.split("<-----TMAILNEXTMAIL----->"):
        seg = seg.strip()
        if not seg:
            continue
        parts = seg.split("<-----TMAILCHOPPER----->", 1)
        list_part = parts[0]
        body_part = parts[1] if len(parts) > 1 else ""

        mid = _RE_LIST_ID.search(list_part)
        if not mid:
            continue
        mid_str = mid.group(1)

        from_list = _pick_div(list_part, "name")
        subject_list = _pick_div(list_part, "subject")
        preview_list = _pick_div(list_part, "body")

        from_body = _pick_div(body_part, "tmail-email-sender")
        time_body = _pick_div(body_part, "tmail-email-time")
        title_body = _pick_div(body_part, "tmail-email-title")
        html_body = _extract_srcdoc(body_part)

        from_addr = from_body or from_list
        subject = title_body or subject_list
        text = preview_list or _strip_tags(html_body)

        attachments = []
        dm = _RE_DOWNLOAD.search(body_part)
        if dm:
            attachments.append(
                EmailAttachment(filename="", url=f"{ORIGIN}{dm.group(1)}")
            )

        out.append(
            Email(
                id=mid_str,
                from_addr=from_addr,
                to=recipient,
                subject=subject,
                text=text,
                html=html_body,
                date=_parse_date(time_body),
                is_read=False,
                attachments=attachments,
            )
        )
    return out


def generate_email(**kwargs) -> EmailInfo:
    resp = tm_http.get(f"{ORIGIN}/api/user?user", headers=DEFAULT_HEADERS)
    resp.raise_for_status()

    nexus = resp.cookies.get("NEXUS_TOKEN") or ""
    if not nexus:
        raise Exception("linshiyou: NEXUS_TOKEN not in Set-Cookie")

    email = (resp.text or "").strip()
    if not email or "@" not in email:
        raise Exception("linshiyou: invalid email in response body")

    token = f"NEXUS_TOKEN={nexus}; tmail-emails={email}"
    return EmailInfo(channel=CHANNEL, email=email, _token=token)


def get_emails(token: str, email: str = "", **kwargs) -> list:
    resp = tm_http.get(
        f"{ORIGIN}/api/mail?unseen=1",
        headers={
            **DEFAULT_HEADERS,
            "Cookie": token,
            "x-requested-with": "XMLHttpRequest",
        },
    )
    resp.raise_for_status()
    return _parse_segments(resp.text, email)
