"""
Anonymmail 渠道 — https://anonymmail.net
POST JSON API，需要 cookie session
"""

import random
import string
from typing import List

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "anonymmail"
BASE = "https://anonymmail.net"

HEADERS = {
    "Accept": "*/*",
    "Content-Type": "application/x-www-form-urlencoded",
    "Referer": "https://anonymmail.net/",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
}


def _random_username(length: int = 10) -> str:
    """生成随机用户名"""
    return "".join(random.choices(string.ascii_lowercase + string.digits, k=length))


def _fetch_domains() -> List[str]:
    """获取可用域名列表"""
    resp = tm_http.post(f"{BASE}/api/getDomains", headers=HEADERS, timeout=15)
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, list):
        return []
    out: List[str] = []
    for item in data:
        if isinstance(item, dict):
            domain = item.get("domain")
            if isinstance(domain, str) and domain.strip():
                out.append(domain.strip())
    return out


def generate_email(channel: str = CHANNEL) -> EmailInfo:
    """创建临时邮箱：先 HEAD 获取 cookie session，再 POST /api/create"""
    # 获取可用域名
    domains = _fetch_domains()
    if not domains:
        raise RuntimeError("anonymmail: no domains available")
    domain = random.choice(domains)
    username = _random_username()
    email = f"{username}@{domain}"

    # HEAD 请求获取 cookie session
    session = tm_http.get_session()
    session.head(f"{BASE}/", headers=HEADERS, timeout=15)

    # POST 创建邮箱
    resp = tm_http.post(
        f"{BASE}/api/create",
        headers=HEADERS,
        data=f"email={email}",
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, dict) or not data.get("success"):
        raise RuntimeError("anonymmail: create inbox failed")
    addr = data.get("email")
    if not addr or not str(addr).strip():
        raise RuntimeError("anonymmail: missing email in response")
    return EmailInfo(channel=channel, email=str(addr).strip())


def get_emails(email: str) -> List[Email]:
    """获取邮件列表"""
    if not (email or "").strip():
        raise ValueError("anonymmail: empty email")
    e = email.strip()
    resp = tm_http.post(
        f"{BASE}/api/get",
        headers=HEADERS,
        data=f"email={e}",
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, dict):
        return []
    # 响应格式: {"email@domain": {"created_at": "...", "emails": [...]}}
    inbox = data.get(e)
    if not isinstance(inbox, dict):
        return []
    rows = inbox.get("emails")
    if not isinstance(rows, list):
        return []
    emails: List[Email] = []
    for raw in rows:
        if not isinstance(raw, dict):
            continue
        # 将 token 字段映射为 id
        normalized = dict(raw)
        if "token" in normalized and "id" not in normalized:
            normalized["id"] = normalized.pop("token")
        # 将 body 字段映射为 html（body 包含 HTML 内容）
        if "body" in normalized and "html" not in normalized:
            normalized["html"] = normalized["body"]
            del normalized["body"]
        emails.append(normalize_email(normalized, e))
    return emails
