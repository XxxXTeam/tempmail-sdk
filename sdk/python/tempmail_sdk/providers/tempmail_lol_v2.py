"""
tempmail.lol V2 渠道实现
API: https://api.tempmail.lol
"""

from urllib.parse import quote
from .. import http as tm_http
from ..types import EmailInfo
from ..normalize import normalize_email

CHANNEL = "tempmail-lol-v2"
API_BASE = "https://api.tempmail.lol"

DEFAULT_HEADERS = {
    "Accept": "application/json",
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
}


def generate_email(**kwargs) -> EmailInfo:
    """创建临时邮箱"""
    resp = tm_http.get(f"{API_BASE}/generate", headers=DEFAULT_HEADERS)
    resp.raise_for_status()
    data = resp.json()

    if not data.get("address") or not data.get("token"):
        raise Exception("tempmail-lol-v2: missing address or token")

    return EmailInfo(
        channel=CHANNEL,
        email=data["address"],
        _token=data["token"],
    )


def get_emails(token: str, email: str = "", **kwargs) -> list:
    """获取邮件列表"""
    resp = tm_http.get(
        f"{API_BASE}/auth/{quote(token, safe='')}",
        headers=DEFAULT_HEADERS,
    )
    resp.raise_for_status()
    data = resp.json()

    mail_list = data.get("email") or []
    if not isinstance(mail_list, list):
        return []

    out = []
    for raw in mail_list:
        out.append(
            normalize_email(
                {
                    "id": raw.get("id") or raw.get("_id", ""),
                    "from": raw.get("from") or raw.get("sender", ""),
                    "to": email,
                    "subject": raw.get("subject", ""),
                    "text": raw.get("body") or raw.get("text", ""),
                    "html": raw.get("html") or raw.get("body", ""),
                    "date": raw.get("date") or raw.get("receivedAt", ""),
                    "isRead": False,
                },
                email,
            )
        )
    return out
