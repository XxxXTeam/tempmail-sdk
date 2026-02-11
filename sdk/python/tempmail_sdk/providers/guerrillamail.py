"""
guerrillamail.com 渠道实现
API: https://api.guerrillamail.com/ajax.php
"""

import requests
from urllib.parse import quote
from ..types import EmailInfo, Email
from ..normalize import normalize_email

CHANNEL = "guerrillamail"
BASE_URL = "https://api.guerrillamail.com/ajax.php"

DEFAULT_HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
}


def generate_email(**kwargs) -> EmailInfo:
    """创建临时邮箱"""
    resp = requests.get(
        f"{BASE_URL}?f=get_email_address&lang=en",
        headers=DEFAULT_HEADERS,
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()

    if not data.get("email_addr") or not data.get("sid_token"):
        raise Exception("Failed to generate email: missing email_addr or sid_token")

    expires_at = None
    if data.get("email_timestamp"):
        expires_at = (data["email_timestamp"] + 3600) * 1000

    return EmailInfo(
        channel=CHANNEL,
        email=data["email_addr"],
        token=data["sid_token"],
        expires_at=expires_at,
    )


def get_emails(token: str, email: str = "", **kwargs) -> list:
    """获取邮件列表"""
    resp = requests.get(
        f"{BASE_URL}?f=check_email&seq=0&sid_token={quote(token)}",
        headers=DEFAULT_HEADERS,
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()

    mail_list = data.get("list") or []
    if not isinstance(mail_list, list):
        return []

    return [normalize_email({
        "id": item.get("mail_id"),
        "from": item.get("mail_from"),
        "to": email,
        "subject": item.get("mail_subject"),
        "text": item.get("mail_body") or item.get("mail_excerpt", ""),
        "html": item.get("mail_body", ""),
        "date": item.get("mail_date", ""),
        "isRead": item.get("mail_read") == 1,
    }, email) for item in mail_list]
