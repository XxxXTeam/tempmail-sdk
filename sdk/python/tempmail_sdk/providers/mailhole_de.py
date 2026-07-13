"""
MailholeDe 渠道 — https://mailhole.de

公共临时邮箱，无需认证。
流程:
  1. GET /api/random → HTML 中包含随机邮箱地址
  2. GET /json/{email} → JSON 数组获取邮件
Token 即邮箱地址本身。
"""

import re
from typing import Any, Dict, List
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "mailhole-de"
BASE_URL = "https://mailhole.de"

# 从 /api/random 返回的 HTML 中提取邮箱地址
_EMAIL_RE = re.compile(r"([a-z0-9.]+@mailhole\.de)")


def _first_str(msg: Dict[str, Any], *keys: str) -> str:
    """按优先级从消息字典中提取首个非空字符串值"""
    for key in keys:
        val = msg.get(key)
        if isinstance(val, str) and val:
            return val
    return ""


def generate_email() -> EmailInfo:
    """创建 mailhole.de 临时邮箱"""
    resp = tm_http.get(
        f"{BASE_URL}/api/random", headers={"Accept": "text/html"}, timeout=15
    )
    resp.raise_for_status()

    match = _EMAIL_RE.search(resp.text)
    if not match:
        raise ValueError("mailhole-de: 无法从响应中解析邮箱地址")

    email = match.group(1)
    return EmailInfo(channel=CHANNEL, email=email, _token=email)


def get_emails(email: str) -> List[Email]:
    """获取 mailhole.de 邮件列表"""
    addr = (email or "").strip()
    if not addr:
        raise ValueError("mailhole-de: 缺少邮箱地址")

    resp = tm_http.get(
        f"{BASE_URL}/json/{quote(addr, safe='')}",
        headers={"Accept": "application/json"},
        timeout=15,
    )
    resp.raise_for_status()

    text = resp.text
    if not text or text == "[]":
        return []

    messages = resp.json()
    if not isinstance(messages, list):
        return []

    emails: List[Email] = []
    for msg in messages:
        if not isinstance(msg, dict):
            continue
        row = {
            "id": msg.get("id") or "",
            "from": _first_str(msg, "sender", "from"),
            "to": addr,
            "subject": msg.get("subject") or "",
            "text": _first_str(msg, "body", "text"),
            "html": _first_str(msg, "html", "body"),
            "received_at": _first_str(msg, "date", "received"),
        }
        emails.append(normalize_email(row, addr))
    return emails
