"""
email10min — https://email10min.com
GET /zh 拿 CSRF + Cookie，POST /messages 获取邮件。
"""

import base64
import json
import re
import time
from typing import List, Optional

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "email10min"
BASE_URL = "https://email10min.com"

BROWSER_HEADERS = {
    "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "Cache-Control": "no-cache",
    "DNT": "1",
    "Pragma": "no-cache",
    "Upgrade-Insecure-Requests": "1",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
    "sec-ch-ua": '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
    "sec-ch-ua-mobile": "?0",
    "sec-ch-ua-platform": '"Windows"',
}

AJAX_HEADERS = {
    "Accept": "application/json, text/plain, */*",
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "Content-Type": "application/x-www-form-urlencoded; charset=UTF-8",
    "Origin": BASE_URL,
    "Referer": f"{BASE_URL}/zh",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
    "X-Requested-With": "XMLHttpRequest",
    "sec-ch-ua": '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
    "sec-ch-ua-mobile": "?0",
    "sec-ch-ua-platform": '"Windows"',
    "sec-fetch-dest": "empty",
    "sec-fetch-mode": "cors",
    "sec-fetch-site": "same-origin",
}

TOK_PREFIX = "e10m:"

_csrf_meta_re = re.compile(r'<meta\s+name="csrf-token"\s+content="([^"]+)"', re.I)
_csrf_input_re = re.compile(r'<input[^>]+name="_token"[^>]+value="([^"]+)"', re.I)
_email_id_re = re.compile(r'id="emailAddress"[^>]*>([^<]+)', re.I)
_email_cls_re = re.compile(r'class="[^"]*email[^"]*"[^>]*>([^<]*@[^<]+)', re.I)
_email_data_re = re.compile(r'data-email="([^"]+@[^"]+)"', re.I)
_email_value_re = re.compile(
    r'value="([a-zA-Z0-9._%+\-]+@[a-zA-Z0-9.\-]+\.[a-zA-Z]{2,})"'
)
_email_json_re = re.compile(r'"mailbox"\s*:\s*"([^"]+@[^"]+)"')
_email_generic_re = re.compile(r"([a-zA-Z0-9._%+\-]+@[a-zA-Z0-9.\-]+\.[a-zA-Z]{2,})")


def _cookie_header_from_response(r) -> str:
    """从响应中提取 Cookie 字符串"""
    parts = []
    for c in r.cookies:
        parts.append(f"{c.name}={c.value}")
    return "; ".join(parts)


def _encode_token(cookie: str, csrf: str) -> str:
    raw = json.dumps({"c": cookie, "t": csrf})
    return TOK_PREFIX + base64.b64encode(raw.encode("utf-8")).decode("utf-8")


def _decode_token(token: str) -> tuple:
    if not token.startswith(TOK_PREFIX):
        raise ValueError("email10min: invalid session token")
    try:
        raw = base64.b64decode(token[len(TOK_PREFIX) :]).decode("utf-8")
    except Exception:
        raise ValueError("email10min: invalid session token")
    data = json.loads(raw)
    cookie = (data.get("c") or "").strip()
    csrf = (data.get("t") or "").strip()
    if not cookie or not csrf:
        raise ValueError("email10min: invalid session token (empty fields)")
    return cookie, csrf


def _extract_csrf(html: str) -> str:
    m = _csrf_meta_re.search(html)
    if m:
        return m.group(1)
    m = _csrf_input_re.search(html)
    if m:
        return m.group(1)
    raise RuntimeError("email10min: 未找到 CSRF token")


def _extract_email(html: str) -> str:
    m = _email_id_re.search(html)
    if m and "@" in m.group(1):
        return m.group(1).strip()
    m = _email_cls_re.search(html)
    if m:
        return m.group(1).strip()
    m = _email_data_re.search(html)
    if m:
        return m.group(1).strip()
    m = _email_value_re.search(html)
    if m:
        return m.group(1).strip()
    m = _email_json_re.search(html)
    if m:
        return m.group(1).strip()
    m = _email_generic_re.search(html)
    if m:
        addr = m.group(1)
        if "email10min" not in addr and "example" not in addr and "google" not in addr:
            return addr.strip()
    raise RuntimeError("email10min: 未找到邮箱地址")


def generate_email() -> EmailInfo:
    """创建 email10min 临时邮箱"""
    r = tm_http.get(f"{BASE_URL}/zh", headers=BROWSER_HEADERS, timeout=15)
    r.raise_for_status()

    cookie = _cookie_header_from_response(r)
    html = r.text

    csrf = _extract_csrf(html)
    email_addr = _extract_email(html)
    token = _encode_token(cookie, csrf)

    return EmailInfo(channel=CHANNEL, email=email_addr, _token=token)


def get_emails(email: str, token: str) -> List[Email]:
    """获取 email10min 邮件列表"""
    cookie, csrf = _decode_token(token)
    ts = str(int(time.time() * 1000))

    h = dict(AJAX_HEADERS)
    h["Cookie"] = cookie

    r = tm_http.post(
        f"{BASE_URL}/messages?{ts}",
        headers=h,
        data=f"_token={csrf}&captcha=",
        timeout=15,
    )
    r.raise_for_status()

    data = r.json()
    messages = data.get("messages") or []
    if not isinstance(messages, list):
        return []

    out: List[Email] = []
    for i, raw in enumerate(messages):
        if not isinstance(raw, dict):
            continue
        msg_id = raw.get("id") or raw.get("message_id") or str(i)
        from_addr = raw.get("from") or raw.get("sender") or ""
        to_addr = raw.get("to") or email
        subject = raw.get("subject") or ""
        text = raw.get("text") or raw.get("body") or ""
        html_content = raw.get("html") or raw.get("body_html") or ""
        date = raw.get("date") or raw.get("created_at") or ""

        flat = {
            "id": str(msg_id),
            "from": from_addr,
            "to": to_addr,
            "subject": subject,
            "text": text,
            "html": html_content,
            "date": date,
            "isRead": False,
        }
        out.append(normalize_email(flat, email))
    return out
