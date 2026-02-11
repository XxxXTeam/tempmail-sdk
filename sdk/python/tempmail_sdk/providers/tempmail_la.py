"""
tempmail.la 渠道实现
API: https://tempmail.la/api
支持分页获取邮件
"""

from .. import http as tm_http
from ..types import EmailInfo, Email
from ..normalize import normalize_email

CHANNEL = "tempmail-la"
BASE_URL = "https://tempmail.la/api"

DEFAULT_HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0",
    "Accept": "application/json, text/plain, */*",
    "Content-Type": "application/json",
    "accept-language": "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "cache-control": "no-cache",
    "dnt": "1",
    "locale": "zh-CN",
    "origin": "https://tempmail.la",
    "platform": "PC",
    "pragma": "no-cache",
    "product": "TEMP_MAIL",
    "referer": "https://tempmail.la/zh-CN/tempmail",
    "sec-ch-ua": '"Not(A:Brand";v="8", "Chromium";v="144", "Microsoft Edge";v="144"',
    "sec-ch-ua-mobile": "?0",
    "sec-ch-ua-platform": '"Windows"',
    "sec-fetch-dest": "empty",
    "sec-fetch-mode": "cors",
    "sec-fetch-site": "same-origin",
}


def generate_email(**kwargs) -> EmailInfo:
    """创建临时邮箱"""
    resp = tm_http.post(
        f"{BASE_URL}/mail/create",
        headers=DEFAULT_HEADERS,
        json={"turnstile": ""},
    )
    resp.raise_for_status()
    data = resp.json()

    if data.get("code") != 0 or not data.get("data"):
        raise Exception("Failed to generate email")

    return EmailInfo(
        channel=CHANNEL,
        email=data["data"]["address"],
        expires_at=data["data"].get("endAt"),
        created_at=data["data"].get("startAt"),
    )


def get_emails(email: str, **kwargs) -> list:
    """获取邮件列表（支持分页）"""
    all_emails = []
    cursor = None
    has_more = True

    while has_more:
        resp = tm_http.post(
            f"{BASE_URL}/mail/box",
            headers=DEFAULT_HEADERS,
            json={"address": email, "cursor": cursor},
        )
        resp.raise_for_status()
        data = resp.json()

        if data.get("code") != 0 or not data.get("data"):
            raise Exception("Failed to get emails")

        rows = data["data"].get("rows") or []
        all_emails.extend(rows)

        if data["data"].get("hasMore") and data["data"].get("cursor"):
            cursor = data["data"]["cursor"]
        else:
            has_more = False

    return [normalize_email(raw, email) for raw in all_emails]
