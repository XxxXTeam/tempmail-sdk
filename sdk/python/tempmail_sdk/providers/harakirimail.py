"""
HarakiriMail 渠道 — https://harakirimail.com
无需认证、无需 Cookie、无需 Token 的简单 REST API 渠道
"""

import random
import string
from typing import List

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "harakirimail"
BASE = "https://harakirimail.com"

HEADERS = {
    "Accept": "application/json, text/plain, */*",
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
}


def _random_name(length: int = 12) -> str:
    """随机生成收件箱名"""
    return "".join(random.choices(string.ascii_lowercase + string.digits, k=length))


def generate_email() -> EmailInfo:
    """创建 harakirimail 临时邮箱"""
    name = _random_name()
    email = f"{name}@harakirimail.com"

    # 可选：调用收件箱接口验证地址可用
    resp = tm_http.get(f"{BASE}/api/v1/inbox/{name}", headers=HEADERS, timeout=15)
    resp.raise_for_status()

    return EmailInfo(channel=CHANNEL, email=email)


def _fetch_body(email_id: str) -> dict:
    """获取单封邮件正文"""
    if not email_id:
        return {}
    try:
        resp = tm_http.get(
            f"{BASE}/api/v1/email/{email_id}", headers=HEADERS, timeout=15
        )
        resp.raise_for_status()
        data = resp.json()
        if isinstance(data, dict):
            return data
    except Exception:
        pass
    return {}


def get_emails(email: str) -> List[Email]:
    """获取 harakirimail 邮件列表"""
    if not (email or "").strip():
        raise ValueError("harakirimail: 邮箱地址为空")
    e = email.strip()

    # 从邮箱地址提取收件箱名
    name = e.rsplit("@", 1)[0] if "@" in e else e

    resp = tm_http.get(f"{BASE}/api/v1/inbox/{name}", headers=HEADERS, timeout=15)
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, dict):
        return []

    rows = data.get("emails")
    if not isinstance(rows, list):
        return []

    emails: List[Email] = []
    for raw in rows:
        if not isinstance(raw, dict):
            continue
        email_id = raw.get("_id", "")
        # 获取单封邮件详情以拿到正文
        detail = _fetch_body(email_id)

        flat = {
            "id": email_id,
            "from": raw.get("from", ""),
            "to": e,
            "subject": raw.get("subject", ""),
            "date": raw.get("received", ""),
            "html": detail.get("body_html") or detail.get("html", ""),
            "text": detail.get("body_text") or detail.get("text", ""),
            "isRead": False,
        }
        emails.append(normalize_email(flat, e))
    return emails
