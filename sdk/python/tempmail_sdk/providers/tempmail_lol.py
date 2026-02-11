"""
tempmail.lol 渠道实现
API: https://api.tempmail.lol/v2
"""

import requests
from urllib.parse import quote
from ..types import EmailInfo, Email
from ..normalize import normalize_email

CHANNEL = "tempmail-lol"
BASE_URL = "https://api.tempmail.lol/v2"

DEFAULT_HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36",
    "Content-Type": "application/json",
    "Origin": "https://tempmail.lol",
    "sec-ch-ua": '"Microsoft Edge";v="143", "Chromium";v="143", "Not A(Brand";v="24"',
    "sec-ch-ua-mobile": "?0",
    "sec-ch-ua-platform": '"Windows"',
    "DNT": "1",
}


def generate_email(domain: str = None, **kwargs) -> EmailInfo:
    """创建临时邮箱，支持指定域名"""
    resp = requests.post(
        f"{BASE_URL}/inbox/create",
        headers=DEFAULT_HEADERS,
        json={"domain": domain, "captcha": None},
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()

    if not data.get("address") or not data.get("token"):
        raise Exception("Failed to generate email")

    return EmailInfo(
        channel=CHANNEL,
        email=data["address"],
        token=data["token"],
    )


def get_emails(token: str, email: str = "", **kwargs) -> list:
    """获取邮件列表"""
    resp = requests.get(
        f"{BASE_URL}/inbox?token={quote(token)}",
        headers=DEFAULT_HEADERS,
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()

    return [normalize_email(raw, email) for raw in (data.get("emails") or [])]
