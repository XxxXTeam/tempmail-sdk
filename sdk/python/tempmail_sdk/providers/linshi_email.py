"""
linshi-email.com 渠道实现
API: https://www.linshi-email.com/api/v1
"""

import time
from urllib.parse import quote
from .. import http as tm_http
from ..types import EmailInfo, Email
from ..normalize import normalize_email

CHANNEL = "linshi-email"
BASE_URL = "https://www.linshi-email.com/api/v1"
API_KEY = "552562b8524879814776e52bc8de5c9f"

DEFAULT_HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36",
    "Content-Type": "application/json",
    "Origin": "https://www.linshi-email.com",
    "Referer": "https://www.linshi-email.com/",
    "sec-ch-ua": '"Microsoft Edge";v="143", "Chromium";v="143", "Not A(Brand";v="24"',
    "sec-ch-ua-mobile": "?0",
    "sec-ch-ua-platform": '"Windows"',
    "DNT": "1",
}


def generate_email(**kwargs) -> EmailInfo:
    """创建临时邮箱"""
    resp = tm_http.post(
        f"{BASE_URL}/email/{API_KEY}",
        headers=DEFAULT_HEADERS,
        json={},
    )
    resp.raise_for_status()
    data = resp.json()

    if data.get("status") != "ok":
        raise Exception("Failed to generate email")

    return EmailInfo(
        channel=CHANNEL,
        email=data["data"]["email"],
        expires_at=data["data"].get("expired"),
    )


def get_emails(email: str, **kwargs) -> list:
    """获取邮件列表"""
    ts = int(time.time() * 1000)
    resp = tm_http.get(
        f"{BASE_URL}/refreshmessage/{API_KEY}/{quote(email)}?t={ts}",
        headers=DEFAULT_HEADERS,
    )
    resp.raise_for_status()
    data = resp.json()

    if data.get("status") != "ok":
        raise Exception("Failed to get emails")

    return [normalize_email(raw, email) for raw in (data.get("list") or [])]
