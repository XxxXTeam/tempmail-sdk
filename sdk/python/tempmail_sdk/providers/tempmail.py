"""
tempmail.ing 渠道实现
API: https://api.tempmail.ing/api
"""

from urllib.parse import quote
from .. import http as tm_http
from ..types import EmailInfo
from ..normalize import normalize_email

CHANNEL = "tempmail"
BASE_URL = "https://api.tempmail.ing/api"

DEFAULT_HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36",
    "Content-Type": "application/json",
    "Referer": "https://tempmail.ing/",
    "sec-ch-ua": '"Microsoft Edge";v="143", "Chromium";v="143", "Not A(Brand";v="24"',
    "sec-ch-ua-mobile": "?0",
    "sec-ch-ua-platform": '"Windows"',
    "DNT": "1",
}


def generate_email(duration: int = 30) -> EmailInfo:
    """创建临时邮箱，支持自定义有效期（分钟）"""
    resp = tm_http.post(
        f"{BASE_URL}/generate",
        headers=DEFAULT_HEADERS,
        json={"duration": duration},
    )
    resp.raise_for_status()
    data = resp.json()

    if not data.get("success"):
        raise Exception("Failed to generate email")

    return EmailInfo(
        channel=CHANNEL,
        email=data["email"]["address"],
        expires_at=data["email"].get("expiresAt"),
        created_at=data["email"].get("createdAt"),
    )


def get_emails(email: str, **kwargs) -> list:
    """获取邮件列表"""
    resp = tm_http.get(
        f"{BASE_URL}/emails/{quote(email)}",
        headers=DEFAULT_HEADERS,
    )
    resp.raise_for_status()
    data = resp.json()

    if not data.get("success"):
        raise Exception("Failed to get emails")

    return [
        normalize_email(_flatten_message(raw, email), email)
        for raw in (data.get("emails") or [])
    ]


def _flatten_message(raw, recipient_email):
    """
    将 tempmail.ing 的原始格式扁平化
    content → html, from_address → from, received_at → date
    """
    return {
        "id": raw.get("id", ""),
        "from": raw.get("from_address") or raw.get("from", ""),
        "to": recipient_email,
        "subject": raw.get("subject", ""),
        "text": raw.get("text", ""),
        "html": raw.get("content") or raw.get("html", ""),
        "date": raw.get("received_at") or raw.get("date", ""),
        "is_read": raw.get("is_read") == 1 or raw.get("is_read") is True,
        "attachments": raw.get("attachments", []),
    }
