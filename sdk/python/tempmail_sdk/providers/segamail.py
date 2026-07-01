"""
Segamail 渠道 — https://segamail.com
POST JSON API，单域名 segamail.com，使用 recover_key 作为 token
"""

from typing import List

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "segamail"
DOMAIN = "segamail.com"
BASE = "https://segamail.com/en"

HEADERS = {
    "Accept": "application/json, text/plain, */*",
    "Content-Type": "application/x-www-form-urlencoded",
    "Referer": "https://segamail.com/en",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
}


def generate_email(channel: str = CHANNEL) -> EmailInfo:
    """
    创建临时邮箱
    POST /en/getEmailAddress 返回包含 address 和 recover_key 的 JSON
    使用 recover_key 作为 token
    """
    resp = tm_http.post(
        f"{BASE}/getEmailAddress",
        headers=HEADERS,
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, dict):
        raise RuntimeError("segamail: unexpected response format")
    addr = data.get("address")
    if not addr or not str(addr).strip():
        raise RuntimeError("segamail: missing address in response")
    recover_key = data.get("recover_key", "")
    return EmailInfo(
        channel=channel,
        email=str(addr).strip(),
        _token=str(recover_key) if recover_key else "",
    )


def get_emails(email: str) -> List[Email]:
    """
    获取邮件列表
    POST /en/getInbox，依赖 session cookie（SDK 共享 HTTP 客户端自动管理）
    """
    if not (email or "").strip():
        raise ValueError("segamail: empty email")
    e = email.strip()
    resp = tm_http.post(
        f"{BASE}/getInbox",
        headers=HEADERS,
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, list):
        return []
    emails: List[Email] = []
    for raw in data:
        if not isinstance(raw, dict):
            continue
        # body 字段映射为 html
        normalized = dict(raw)
        if "body" in normalized and "html" not in normalized:
            normalized["html"] = normalized["body"]
            del normalized["body"]
        emails.append(normalize_email(normalized, e))
    return emails
