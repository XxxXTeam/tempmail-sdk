"""
TempMail Plus 渠道 — https://tempmail.plus
无需认证、无需 Cookie、无需 Token 的简单 REST API 渠道
域名: mailto.plus
"""

import random
import string
from typing import List

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "tempmail-plus"
API_BASE = "https://tempmail.plus/api/mails"
DOMAIN = "mailto.plus"

HEADERS = {
    "Accept": "application/json",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"
    ),
    "Referer": "https://tempmail.plus/",
    "Origin": "https://tempmail.plus",
}


def _random_local(length: int = 12) -> str:
    """随机生成本地部分"""
    return "".join(random.choices(string.ascii_lowercase + string.digits, k=length))


def generate_email() -> EmailInfo:
    """创建 tempmail-plus 临时邮箱"""
    local = _random_local()
    email = f"{local}@{DOMAIN}"

    # 调用列表接口验证地址可用
    resp = tm_http.get(f"{API_BASE}/?email={email}&epin=", headers=HEADERS, timeout=15)
    resp.raise_for_status()

    return EmailInfo(channel=CHANNEL, email=email)


def _fetch_body(mail_id: int, email: str) -> dict:
    """获取单封邮件正文"""
    if not mail_id:
        return {}
    try:
        resp = tm_http.get(
            f"{API_BASE}/{mail_id}?email={email}&epin=",
            headers=HEADERS,
            timeout=15,
        )
        resp.raise_for_status()
        data = resp.json()
        if isinstance(data, dict):
            return data
    except Exception:
        pass
    return {}


def get_emails(email: str) -> List[Email]:
    """获取 tempmail-plus 邮件列表"""
    if not (email or "").strip():
        raise ValueError("tempmail-plus: 邮箱地址为空")
    e = email.strip()

    resp = tm_http.get(f"{API_BASE}/?email={e}&epin=", headers=HEADERS, timeout=15)
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, dict):
        return []

    rows = data.get("mail_list")
    if not isinstance(rows, list):
        return []

    emails: List[Email] = []
    for raw in rows:
        if not isinstance(raw, dict):
            continue
        mail_id = raw.get("mail_id", 0)
        # 获取单封邮件详情以拿到正文
        detail = _fetch_body(mail_id, e)

        flat = {
            "id": str(mail_id) if mail_id else "",
            "from": raw.get("from_mail") or raw.get("from_name", ""),
            "to": e,
            "subject": raw.get("subject", ""),
            "date": raw.get("time", ""),
            "html": detail.get("html", ""),
            "text": detail.get("text", ""),
            "isRead": not raw.get("is_new", True),
        }
        emails.append(normalize_email(flat, e))
    return emails
