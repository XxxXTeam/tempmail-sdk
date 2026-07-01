"""
DisposableMail.app 渠道 — https://disposablemail.app
纯 REST JSON API，无需认证。
创建收件箱: POST /api/inbox，返回 address + token。
获取邮件: GET /api/inbox/emails?token={token}。
域名: @disposablemail.dev, @mailmehere.cc
"""

from typing import List

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "disposablemail-app"
BASE = "https://disposablemail.app"

# 公共请求头
_HEADERS = {
    "Content-Type": "application/json",
    "Accept": "application/json",
    "Referer": f"{BASE}/",
    "Origin": BASE,
}


def generate_email(channel: str = CHANNEL) -> EmailInfo:
    """
    创建 disposablemail.app 临时邮箱

    流程:
    1. POST /api/inbox 创建收件箱
    2. 从响应中提取 address 和 token
    3. token 直接存储 API 返回的字符串
    """
    resp = tm_http.post(
        f"{BASE}/api/inbox",
        headers=_HEADERS,
        json={},
    )
    resp.raise_for_status()

    body = resp.json()
    address = body.get("address", "")
    token = body.get("token", "")

    if not address or "@" not in address:
        raise RuntimeError(f"disposablemail-app: 返回的邮箱地址无效: {address!r}")

    if not token:
        raise RuntimeError("disposablemail-app: 返回的 token 为空")

    return EmailInfo(channel=channel, email=address, _token=token)


def get_emails(email: str, token: str) -> List[Email]:
    """
    获取 disposablemail.app 邮件列表

    流程:
    1. GET /api/inbox/emails?token={token} 获取邮件
    2. 使用 normalize_email 标准化返回结果
    """
    if not token:
        raise ValueError("disposablemail-app: token 不能为空")

    addr = (email or "").strip()
    if not addr:
        raise ValueError("disposablemail-app: 邮箱地址不能为空")

    resp = tm_http.get(
        f"{BASE}/api/inbox/emails",
        headers={"Accept": "application/json", "Referer": f"{BASE}/"},
        params={"token": token},
    )
    resp.raise_for_status()

    body = resp.json()
    raw_emails = body.get("emails", [])
    if not isinstance(raw_emails, list):
        return []

    out: List[Email] = []
    for item in raw_emails:
        if not isinstance(item, dict):
            continue
        raw = {
            "id": str(item.get("id", "")),
            "from": item.get("from", item.get("from_address", item.get("sender", ""))),
            "to": item.get("to", addr),
            "subject": item.get("subject", ""),
            "text": item.get("text", item.get("body_text", "")),
            "html": item.get("html", item.get("body_html", item.get("body", ""))),
            "date": item.get("date", item.get("created_at", item.get("receivedAt", ""))),
        }
        out.append(normalize_email(raw, addr))

    return out
