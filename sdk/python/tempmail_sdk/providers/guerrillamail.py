"""
guerrillamail.com 渠道实现
API: https://api.guerrillamail.com/ajax.php
"""

import re
from urllib.parse import quote
from .. import http as tm_http
from ..types import EmailInfo
from ..normalize import normalize_email

CHANNEL = "guerrillamail"
BASE_URL = "https://api.guerrillamail.com/ajax.php"

DEFAULT_HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
}


def generate_email(**kwargs) -> EmailInfo:
    """创建临时邮箱"""
    resp = tm_http.get(
        f"{BASE_URL}?f=get_email_address&lang=en",
        headers=DEFAULT_HEADERS,
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
        _token=data["sid_token"],
        expires_at=expires_at,
    )


def get_emails(token: str, email: str = "", **kwargs) -> list:
    """获取邮件列表，对每封邮件调用 fetch_email 获取完整正文"""
    resp = tm_http.get(
        f"{BASE_URL}?f=check_email&seq=0&sid_token={quote(token)}",
        headers=DEFAULT_HEADERS,
    )
    resp.raise_for_status()
    data = resp.json()

    mail_list = data.get("list") or []
    if not isinstance(mail_list, list):
        return []

    out = []
    for item in mail_list:
        body = item.get("mail_body") or ""
        if not body and item.get("mail_id"):
            try:
                dr = tm_http.get(
                    f"{BASE_URL}?f=fetch_email&sid_token={quote(token)}&email_id={quote(str(item['mail_id']))}",
                    headers=DEFAULT_HEADERS,
                )
                if dr.ok:
                    detail = dr.json()
                    body = detail.get("mail_body") or ""
            except Exception:  # noqa: BLE001
                pass
        text = item.get("mail_excerpt") or re.sub(r"<[^>]+>", " ", body).strip()
        text = re.sub(r"\s+", " ", text).strip()
        out.append(normalize_email({
            "id": item.get("mail_id"),
            "from": item.get("mail_from"),
            "to": email,
            "subject": item.get("mail_subject"),
            "text": text,
            "html": body,
            "date": item.get("mail_date", ""),
            "isRead": item.get("mail_read") == 1,
        }, email))
    return out
