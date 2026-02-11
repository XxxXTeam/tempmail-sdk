"""
awamail.com 渠道实现
API: https://awamail.com/welcome
需要保存 session cookie 用于后续获取邮件
"""

import requests
from ..types import EmailInfo, Email
from ..normalize import normalize_email

CHANNEL = "awamail"
BASE_URL = "https://awamail.com/welcome"

DEFAULT_HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0",
    "Accept": "application/json, text/javascript, */*; q=0.01",
    "accept-language": "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "cache-control": "no-cache",
    "dnt": "1",
    "origin": "https://awamail.com",
    "pragma": "no-cache",
    "referer": "https://awamail.com/?lang=zh",
    "sec-ch-ua": '"Not(A:Brand";v="8", "Chromium";v="144", "Microsoft Edge";v="144"',
    "sec-ch-ua-mobile": "?0",
    "sec-ch-ua-platform": '"Windows"',
    "sec-fetch-dest": "empty",
    "sec-fetch-mode": "cors",
    "sec-fetch-site": "same-origin",
}


def generate_email(**kwargs) -> EmailInfo:
    """创建临时邮箱，从响应中提取 awamail_session cookie"""
    resp = requests.post(
        f"{BASE_URL}/change_mailbox",
        headers={**DEFAULT_HEADERS, "Content-Length": "0"},
        timeout=15,
    )
    resp.raise_for_status()

    # 提取 session cookie
    session_cookie = ""
    for name, value in resp.cookies.items():
        if name == "awamail_session":
            session_cookie = f"awamail_session={value}"
            break

    if not session_cookie:
        raise Exception("Failed to extract session cookie")

    data = resp.json()
    if not data.get("success") or not data.get("data"):
        raise Exception("Failed to generate email")

    return EmailInfo(
        channel=CHANNEL,
        email=data["data"]["email_address"],
        token=session_cookie,
        expires_at=data["data"].get("expired_at"),
        created_at=data["data"].get("created_at"),
    )


def get_emails(token: str, email: str = "", **kwargs) -> list:
    """获取邮件列表，需要传入 session cookie"""
    resp = requests.get(
        f"{BASE_URL}/get_emails",
        headers={
            **DEFAULT_HEADERS,
            "Cookie": token,
            "x-requested-with": "XMLHttpRequest",
        },
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()

    if not data.get("success") or not data.get("data"):
        raise Exception("Failed to get emails")

    return [normalize_email(raw, email) for raw in (data["data"].get("emails") or [])]
