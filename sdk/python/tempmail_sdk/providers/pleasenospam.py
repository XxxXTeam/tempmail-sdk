"""
PleeaseNoSpam 渠道 — https://pleasenospam.email
无需认证、无需 Token，本地生成邮箱地址，通过 JSON API 获取邮件
支持域名: pleasenospam.email, spamlessmail.org
"""

import random
import string
from typing import List, Optional

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "pleasenospam"
BASE = "https://pleasenospam.email"

# 支持的域名列表
DOMAINS = ["pleasenospam.email", "spamlessmail.org"]

HEADERS = {
    "Accept": "application/json, text/plain, */*",
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
}


def _random_local(length: int = 12) -> str:
    """随机生成邮箱本地部分"""
    return "".join(random.choices(string.ascii_lowercase + string.digits, k=length))


def _pick_domain(preferred: Optional[str] = None) -> str:
    """选择域名，优先使用指定域名，否则默认 pleasenospam.email"""
    if preferred and preferred.strip():
        p = preferred.strip().lower()
        for d in DOMAINS:
            if d.lower() == p:
                return d
        raise ValueError(f"pleasenospam: 不支持的域名: {p}")
    return DOMAINS[0]


def generate_email(domain: Optional[str] = None, channel: str = CHANNEL) -> EmailInfo:
    """
    创建 pleasenospam 临时邮箱
    无需调用 API，直接本地生成 {randomLocal}@{domain} 格式地址
    """
    d = _pick_domain(domain)
    local = _random_local()
    email = f"{local}@{d}"
    return EmailInfo(channel=channel, email=email)


def get_emails(email: str) -> List[Email]:
    """
    获取 pleasenospam 邮件列表
    API: GET https://pleasenospam.email/{email}.json
    返回 JSON 数组，from 字段为数组格式，取 from[0] 作为发件人
    """
    if not (email or "").strip():
        raise ValueError("pleasenospam: 邮箱地址为空")
    e = email.strip()

    resp = tm_http.get(f"{BASE}/{e}.json", headers=HEADERS, timeout=15)
    resp.raise_for_status()
    data = resp.json()

    if not isinstance(data, list):
        return []

    emails: List[Email] = []
    for raw in data:
        if not isinstance(raw, dict):
            continue

        # from 字段是数组，取第一个元素作为发件人地址
        from_field = raw.get("from", [])
        from_addr = ""
        if isinstance(from_field, list) and len(from_field) > 0:
            from_addr = str(from_field[0])
        elif isinstance(from_field, str):
            from_addr = from_field

        # receivedDate 是秒级时间戳
        received_date = raw.get("receivedDate")

        flat = {
            "id": raw.get("id", ""),
            "from": from_addr,
            "to": e,
            "subject": raw.get("subject", ""),
            "date": received_date,
            "text": raw.get("text", ""),
            "html": raw.get("html", ""),
            "isRead": False,
        }
        emails.append(normalize_email(flat, e))
    return emails
