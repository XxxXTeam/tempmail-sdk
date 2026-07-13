"""
Mailnesia 渠道 — https://mailnesia.com
"""

import html as html_lib
import random
import re
import string
from typing import Any, Dict, List
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "mailnesia"
BASE_URL = "https://mailnesia.com"
DOMAIN = "mailnesia.com"
HEADERS = {"Accept": "text/html,*/*"}

_ROW_RE = re.compile(
    r'<tr\s+id="([^"]+)"[^>]*class="[^"]*\bemailheader\b[^"]*"[^>]*>(.*?)</tr>',
    re.I | re.S,
)
_TIME_RE = re.compile(r'<time\s+datetime="([^"]+)"', re.I | re.S)
_ANCHOR_RE = re.compile(r'<a\b[^>]*class="email"[^>]*>(.*?)</a>', re.I | re.S)
_TAG_RE = re.compile(r"<[^>]+>", re.S)


def _random_local() -> str:
    chars = string.ascii_lowercase + string.digits
    return "sdk" + "".join(random.choice(chars) for _ in range(16))


def _local_part(email: str) -> str:
    return (email or "").strip().split("@")[0].strip()


def _mailbox_url(local: str) -> str:
    return f"{BASE_URL}/mailbox/{quote(local, safe='')}"


def _detail_url(local: str, message_id: str) -> str:
    return f"{_mailbox_url(local)}/{quote(message_id, safe='')}"


def _fetch_html(url: str) -> str:
    resp = tm_http.get(url, headers=HEADERS, timeout=15)
    resp.raise_for_status()
    return resp.text


def _clean_text(raw: str) -> str:
    return " ".join(html_lib.unescape(_TAG_RE.sub(" ", raw or "")).split())


def _parse_rows(page: str) -> List[Dict[str, Any]]:
    rows: List[Dict[str, Any]] = []
    for match in _ROW_RE.finditer(page):
        message_id = match.group(1).strip()
        row_html = match.group(2)
        date_match = _TIME_RE.search(row_html)
        anchors = [_clean_text(m.group(1)) for m in _ANCHOR_RE.finditer(row_html)]
        if len(anchors) < 3:
            continue
        rows.append(
            {
                "id": message_id,
                "date": (
                    html_lib.unescape(date_match.group(1).strip()) if date_match else ""
                ),
                "from": anchors[0],
                "to": anchors[1],
                "subject": anchors[2],
            }
        )
    return rows


def _extract_div_by_id(page: str, div_id: str, next_id: str = "") -> str:
    needle = f'id="{div_id}"'
    pos = page.find(needle)
    if pos < 0:
        return ""
    open_end = page.find(">", pos)
    if open_end < 0:
        return ""
    start = open_end + 1
    end = -1
    if next_id:
        end = page.find(f'<div id="{next_id}"', start)
    if end < 0:
        close = page.find("</div>", start)
        if close >= 0:
            end = close + len("</div>")
    if end < 0:
        return ""
    content = page[start:end].strip()
    if content.endswith("</div>"):
        content = content[: -len("</div>")].strip()
    return content


def _extract_plain(page: str, message_id: str) -> str:
    pattern = re.compile(
        rf'<div\s+id="text_plain_{re.escape(message_id)}"[^>]*>\s*<pre>(.*?)</pre>\s*</div>',
        re.I | re.S,
    )
    match = pattern.search(page)
    return html_lib.unescape(match.group(1)).strip() if match else ""


def _fetch_detail(local: str, row: Dict[str, Any]) -> Dict[str, Any]:
    message_id = str(row.get("id") or "").strip()
    if not message_id:
        return row
    page = _fetch_html(_detail_url(local, message_id))
    detail = dict(row)
    detail["text"] = _extract_plain(page, message_id)
    detail["html"] = _extract_div_by_id(
        page, f"text_html_{message_id}", f"text_plain_{message_id}"
    )
    return detail


def generate_email() -> EmailInfo:
    local = _random_local()
    _fetch_html(_mailbox_url(local))
    return EmailInfo(channel=CHANNEL, email=f"{local}@{DOMAIN}")


def get_emails(email: str) -> List[Email]:
    local = _local_part(email)
    if not local:
        raise ValueError("mailnesia: empty email")

    page = _fetch_html(_mailbox_url(local))
    rows = _parse_rows(page)
    emails: List[Email] = []
    for row in rows:
        try:
            emails.append(normalize_email(_fetch_detail(local, row), email))
        except Exception:
            emails.append(normalize_email(row, email))
    return emails
