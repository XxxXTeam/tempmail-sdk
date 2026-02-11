"""
mail.chatgpt.org.uk 渠道实现
API: https://mail.chatgpt.org.uk/api
"""

import requests
from urllib.parse import quote
from ..types import EmailInfo, Email
from ..normalize import normalize_email

CHANNEL = "chatgpt-org-uk"
BASE_URL = "https://mail.chatgpt.org.uk/api"

DEFAULT_HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36",
    "Content-Type": "application/json",
    "Accept": "*/*",
    "Referer": "https://mail.chatgpt.org.uk/",
    "sec-ch-ua": '"Microsoft Edge";v="143", "Chromium";v="143", "Not A(Brand";v="24"',
    "sec-ch-ua-mobile": "?0",
    "sec-ch-ua-platform": '"Windows"',
    "DNT": "1",
}


def generate_email(**kwargs) -> EmailInfo:
    """创建临时邮箱"""
    resp = requests.get(
        f"{BASE_URL}/generate-email",
        headers=DEFAULT_HEADERS,
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()

    if not data.get("success"):
        raise Exception("Failed to generate email")

    return EmailInfo(
        channel=CHANNEL,
        email=data["data"]["email"],
    )


def get_emails(email: str, **kwargs) -> list:
    """获取邮件列表"""
    resp = requests.get(
        f"{BASE_URL}/emails?email={quote(email)}",
        headers=DEFAULT_HEADERS,
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()

    if not data.get("success"):
        raise Exception("Failed to get emails")

    return [normalize_email(raw, email) for raw in (data.get("data", {}).get("emails") or [])]
