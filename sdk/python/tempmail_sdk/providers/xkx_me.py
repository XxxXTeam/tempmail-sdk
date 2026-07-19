"""
xkx-me 渠道 — https://xkx.me

中文临时邮箱服务，使用 CSRF token + session cookie 认证。
  1. 创建: 获取 session → POST /mailbox/create/random → 从 redirect 提取邮箱
  2. 读信: GET /mailbox/{email}/messages (携带 session cookie，Accept: application/json)
"""

import re
from typing import List
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "xkx-me"
BASE_URL = "https://xkx.me"
HEADERS = {
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36"
    ),
}


def _get_session() -> tuple:
    """获取 CSRF token 和 session cookies"""
    resp = tm_http.get(BASE_URL, headers=HEADERS, timeout=15)
    resp.raise_for_status()

    csrf_match = re.search(r'csrf-token" content="([^"]+)"', resp.text)
    if not csrf_match:
        raise RuntimeError("xkx-me: 无法获取 CSRF token")

    cookies = resp.cookies
    return csrf_match.group(1), cookies


def generate_email() -> EmailInfo:
    """
    创建 xkx.me 临时邮箱
    1. GET / 获取 CSRF token 和 session cookie
    2. POST /mailbox/create/random 创建邮箱
    3. 从 302 redirect Location 提取邮箱地址
    """
    csrf, cookies = _get_session()

    resp = tm_http.post(
        f"{BASE_URL}/mailbox/create/random",
        headers=HEADERS,
        data={"_token": csrf},
        cookies=cookies,
        timeout=15,
        allow_redirects=False,
    )

    location = resp.headers.get("Location", "")
    email_match = re.search(r"mailbox/([^/\s\"'<>]+@xkx\.me)", location)
    if not email_match:
        raise RuntimeError("xkx-me: 无法从响应中提取邮箱地址")

    email = email_match.group(1)

    cookie_str = "; ".join(
        f"{k}={v}" for k, v in cookies.items()
    )

    return EmailInfo(channel=CHANNEL, email=email, _token=cookie_str)


def get_emails(token: str, email: str) -> List[Email]:
    """
    获取 xkx.me 邮件列表
    使用 session cookie 访问 /mailbox/{email}/messages 获取 JSON 响应
    """
    address = (email or "").strip()
    if not address:
        raise ValueError("xkx-me: 缺少邮箱地址")

    cookie_dict = {}
    if token:
        for part in token.split("; "):
            if "=" in part:
                k, v = part.split("=", 1)
                cookie_dict[k.strip()] = v.strip()

    headers = {
        **HEADERS,
        "Accept": "application/json",
        "X-Requested-With": "XMLHttpRequest",
    }

    resp = tm_http.get(
        f"{BASE_URL}/mailbox/{quote(address, safe='@')}/messages",
        headers=headers,
        cookies=cookie_dict,
        timeout=15,
    )

    if resp.status_code == 404:
        return []
    resp.raise_for_status()

    data = resp.json()
    if not data or not isinstance(data, (dict, list)):
        return []

    if isinstance(data, list):
        message_list = data
    elif isinstance(data, dict):
        if "messages" in data:
            message_list = data["messages"]
        elif "message" in data and isinstance(data["message"], dict):
            message_list = [data["message"]]
        else:
            return []
    else:
        return []

    emails: List[Email] = []
    for msg in message_list:
        if not isinstance(msg, dict):
            continue
        raw = {
            "id": msg.get("id", ""),
            "from": msg.get("from", ""),
            "to": address,
            "subject": msg.get("subject", ""),
            "date": msg.get("date", ""),
            "html": msg.get("html", msg.get("body", "")),
            "text": msg.get("text", ""),
            "is_read": False,
            "attachments": [],
        }
        emails.append(normalize_email(raw, address))

    return emails
