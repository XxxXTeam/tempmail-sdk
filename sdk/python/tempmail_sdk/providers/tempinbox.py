"""
TempInbox 渠道 — https://www.tempinbox.xyz
"""

import random
import string
from typing import List, Optional

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "tempinbox"
BASE = "https://endpoint.tempinbox.xyz"

# 支持的域名列表
DOMAINS = ["tempinbox.xyz", "thepiratebay.cloud", "cryptoblad.nl"]

HEADERS = {
    "Accept": "application/json, text/plain, */*",
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
    "Cache-Control": "no-cache",
    "DNT": "1",
    "Pragma": "no-cache",
    "Referer": "https://www.tempinbox.xyz/",
    "sec-ch-ua": '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
    "sec-ch-ua-mobile": "?0",
    "sec-ch-ua-platform": '"Windows"',
    "sec-fetch-dest": "empty",
    "sec-fetch-mode": "cors",
    "sec-fetch-site": "cross-site",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
}


def _random_user(length: int = 10) -> str:
    """生成随机用户名"""
    return "".join(random.choices(string.ascii_lowercase + string.digits, k=length))


def generate_email(domain: Optional[str] = None, channel: str = CHANNEL) -> EmailInfo:
    """
    创建临时邮箱
    如果指定了域名则使用自定义邮箱端点，否则使用随机邮箱端点
    """
    if domain and domain.strip():
        # 指定域名时，验证域名是否可用
        d = domain.strip().lower()
        if d not in DOMAINS:
            raise ValueError(f"tempinbox: domain not available: {d}")
        # 使用自定义邮箱端点
        user = _random_user()
        addr = f"{user}@{d}"
        resp = tm_http.get(f"{BASE}/email/{addr}", headers=HEADERS, timeout=15)
        resp.raise_for_status()
    else:
        # 随机邮箱
        resp = tm_http.get(f"{BASE}/email/Random", headers=HEADERS, timeout=15)
        resp.raise_for_status()
        # 返回值为纯字符串（带引号），如 "user@domain"
        addr = resp.text.strip().strip('"')

    if not addr or "@" not in addr:
        raise RuntimeError("tempinbox: invalid email address returned")

    return EmailInfo(channel=channel, email=addr)


def get_emails(email: str) -> List[Email]:
    """获取邮件列表"""
    if not (email or "").strip():
        raise ValueError("tempinbox: empty email")
    e = email.strip()
    resp = tm_http.get(
        f"{BASE}/messages/{e}",
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
