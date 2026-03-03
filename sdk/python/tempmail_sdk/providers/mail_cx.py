"""
mail.cx 渠道实现
API 文档: https://api.mail.cx/

无需注册，任意邮箱名即可接收邮件
"""

import random
import string
from ..types import EmailInfo, Email
from ..normalize import normalize_email
from .. import http as tm_http

CHANNEL = "mail-cx"
BASE_URL = "https://api.mail.cx/api/v1"
DOMAINS = ["qabq.com", "nqmo.com"]

DEFAULT_HEADERS = {
    "Content-Type": "application/json",
    "Accept": "application/json",
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
}


def _random_username(length=8):
    return "".join(random.choices(string.ascii_lowercase + string.digits, k=length))


def _get_token():
    """获取 JWT Token"""
    resp = tm_http.post(
        f"{BASE_URL}/auth/authorize_token",
        headers=DEFAULT_HEADERS,
        json={},
    )
    resp.raise_for_status()
    token = resp.text.strip().strip('"')
    return token


def generate_email():
    """创建临时邮箱"""
    token = _get_token()
    domain = random.choice(DOMAINS)
    username = _random_username()
    email = f"{username}@{domain}"

    return EmailInfo(
        channel=CHANNEL,
        email=email,
        _token=token,
    )


def get_emails(token, email="", **kwargs):
    """获取邮件列表"""
    from urllib.parse import quote
    resp = tm_http.get(
        f"{BASE_URL}/mailbox/{quote(email, safe='')}",
        headers={**DEFAULT_HEADERS, "Authorization": f"Bearer {token}"},
    )
    resp.raise_for_status()
    messages = resp.json()

    if not isinstance(messages, list) or not messages:
        return []

    emails = []
    for msg in messages:
        try:
            detail_resp = tm_http.get(
                f"{BASE_URL}/mailbox/{quote(email, safe='')}/{msg['id']}",
                headers={**DEFAULT_HEADERS, "Authorization": f"Bearer {token}"},
            )
            detail_resp.raise_for_status()
            detail = detail_resp.json()
            emails.append(normalize_email(detail, email))
        except Exception:
            emails.append(normalize_email(msg, email))

    return emails
