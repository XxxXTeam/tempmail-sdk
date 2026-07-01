"""
Byom 渠道 — https://byom.de
纯 GET 无认证，直接生成随机用户名
"""

import random
import string
from typing import List

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "byom"
DOMAIN = "byom.de"
BASE = "https://api.byom.de"

HEADERS = {
    "Accept": "application/json, text/plain, */*",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
}


def _random_username(length: int = 10) -> str:
    """生成随机用户名"""
    return "".join(random.choices(string.ascii_lowercase + string.digits, k=length))


def generate_email(channel: str = CHANNEL) -> EmailInfo:
    """生成随机邮箱地址，无需 API 调用"""
    username = _random_username()
    email = f"{username}@{DOMAIN}"
    return EmailInfo(channel=channel, email=email)


def get_emails(email: str) -> List[Email]:
    """获取邮件列表"""
    if not (email or "").strip():
        raise ValueError("byom: empty email")
    e = email.strip()
    # 提取用户名部分
    username = e.split("@")[0]
    if not username:
        raise ValueError("byom: invalid email format")
    resp = tm_http.get(
        f"{BASE}/mails/{username}",
        headers=HEADERS,
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, list):
        return []
    emails: List[Email] = []
    for raw in data:
        if not isinstance(raw, dict):
            continue
        emails.append(normalize_email(raw, e))
    return emails
