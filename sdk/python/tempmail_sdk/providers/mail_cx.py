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
DOMAINS = ["qabq.com", "nqmo.com", "end.tw", "uuf.me", "6n9.net"]

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


def _extract_email(s):
    """
    从 "name <email>" 格式中提取邮箱地址
    如 "openel <openel@foxmail.com>" → "openel@foxmail.com"
    """
    import re
    if not s:
        return ""
    match = re.search(r"<([^>]+)>", s)
    return match.group(1) if match else s


def _flatten_message(msg, recipient_email):
    """
    将 mail.cx 响应扁平化为 normalize_email 可处理的格式
    mail.cx 的 from 是 "name <email>" 格式，to 是字符串数组
    """
    to_list = msg.get("to", [])
    to_addr = _extract_email(to_list[0]) if isinstance(to_list, list) and to_list else recipient_email

    return {
        "id": msg.get("id", ""),
        "from": _extract_email(msg.get("from", "")),
        "to": to_addr,
        "subject": msg.get("subject", ""),
        "text": msg.get("text", "") or msg.get("body", ""),
        "html": msg.get("html", ""),
        "date": msg.get("date", ""),
        "seen": msg.get("seen", False),
        "attachments": msg.get("attachments", []),
    }


def get_emails(token, email="", **kwargs):
    """获取邮件列表（每次刷新token，mail.cx token有效期仅~5分钟）"""
    fresh_token = _get_token()
    resp = tm_http.get(
        f"{BASE_URL}/mailbox/{email}",
        headers={**DEFAULT_HEADERS, "Authorization": f"Bearer {fresh_token}"},
    )
    resp.raise_for_status()
    messages = resp.json()

    if not isinstance(messages, list) or not messages:
        return []

    emails = []
    for msg in messages:
        try:
            detail_resp = tm_http.get(
                f"{BASE_URL}/mailbox/{email}/{msg['id']}",
                headers={**DEFAULT_HEADERS, "Authorization": f"Bearer {fresh_token}"},
            )
            detail_resp.raise_for_status()
            detail = detail_resp.json()
            emails.append(normalize_email(_flatten_message(detail, email), email))
        except Exception:
            emails.append(normalize_email(_flatten_message(msg, email), email))

    return emails
