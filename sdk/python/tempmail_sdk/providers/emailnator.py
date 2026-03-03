"""
Emailnator 渠道实现
网站: https://www.emailnator.com

需要 XSRF-TOKEN Session 认证
"""

import json
from ..types import EmailInfo, Email
from ..normalize import normalize_email
from .. import http as tm_http

CHANNEL = "emailnator"
BASE_URL = "https://www.emailnator.com"

DEFAULT_HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/145.0.0.0 Safari/537.36",
    "Accept": "application/json",
    "Content-Type": "application/json",
    "X-Requested-With": "XMLHttpRequest",
}


def _init_session():
    """初始化 Session，获取 XSRF-TOKEN 和 Cookie"""
    import requests
    session = requests.Session()
    session.headers.update({"User-Agent": DEFAULT_HEADERS["User-Agent"]})
    resp = session.get(BASE_URL, timeout=15)
    resp.raise_for_status()

    xsrf_token = session.cookies.get("XSRF-TOKEN", "")
    if not xsrf_token:
        raise Exception("Failed to extract XSRF-TOKEN")

    cookie_str = "; ".join([f"{k}={v}" for k, v in session.cookies.items()])
    return xsrf_token, cookie_str


def generate_email():
    """创建临时邮箱"""
    xsrf_token, cookies = _init_session()

    resp = tm_http.post(
        f"{BASE_URL}/generate-email",
        headers={
            **DEFAULT_HEADERS,
            "X-XSRF-TOKEN": xsrf_token,
            "Cookie": cookies,
        },
        json={"email": ["domain"]},
    )
    resp.raise_for_status()
    data = resp.json()

    email_list = data.get("email", [])
    if not email_list:
        raise Exception("Failed to generate email: empty response")

    token_payload = json.dumps({"xsrfToken": xsrf_token, "cookies": cookies})

    return EmailInfo(
        channel=CHANNEL,
        email=email_list[0],
        _token=token_payload,
    )


def get_emails(token, email="", **kwargs):
    """获取邮件列表"""
    session = json.loads(token)
    xsrf_token = session["xsrfToken"]
    cookies = session["cookies"]

    resp = tm_http.post(
        f"{BASE_URL}/message-list",
        headers={
            **DEFAULT_HEADERS,
            "X-XSRF-TOKEN": xsrf_token,
            "Cookie": cookies,
        },
        json={"email": email},
    )
    resp.raise_for_status()
    data = resp.json()
    message_data = data.get("messageData", [])

    # 过滤广告消息
    emails = []
    for msg in message_data:
        msg_id = msg.get("messageID", "")
        if msg_id.startswith("ADS"):
            continue
        emails.append(normalize_email({
            "id": msg_id,
            "from": msg.get("from", ""),
            "to": email,
            "subject": msg.get("subject", ""),
            "text": "",
            "html": "",
            "date": msg.get("time", ""),
            "isRead": False,
            "attachments": [],
        }, email))

    return emails
