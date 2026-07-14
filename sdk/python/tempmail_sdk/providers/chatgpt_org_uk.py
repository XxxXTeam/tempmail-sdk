"""
mail.chatgpt.org.uk 渠道实现
API: https://mail.chatgpt.org.uk/api

流程:
1. GET /api/domains/public → 可用域名列表
2. 本地生成随机用户名，组合邮箱地址
3. POST /api/inbox-token → JWT token + gm_sid cookie
4. GET /api/emails?email=xxx → 邮件列表
"""

import json
import random
import string
from urllib.parse import quote

from .. import http as tm_http
from ..types import EmailInfo
from ..normalize import normalize_email

CHANNEL = "chatgpt-org-uk"
BASE_URL = "https://mail.chatgpt.org.uk/api"

DEFAULT_HEADERS = {
    "User-Agent": "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/150.0.0.0 Safari/537.36 Edg/150.0.0.0",
    "Accept": "*/*",
    "Referer": "https://mail.chatgpt.org.uk/zh/",
    "Origin": "https://mail.chatgpt.org.uk",
    "DNT": "1",
    "sec-fetch-dest": "empty",
    "sec-fetch-mode": "cors",
    "sec-fetch-site": "same-origin",
}


def _random_username(length: int = 10) -> str:
    return "".join(random.choices(string.ascii_lowercase + string.digits, k=length))


def _extract_gm_sid(resp) -> str:
    for name, value in resp.cookies.items():
        if name == "gm_sid":
            return value
    return ""


def _fetch_domains() -> list[str]:
    """获取可用域名列表"""
    resp = tm_http.get(f"{BASE_URL}/domains/public", headers=DEFAULT_HEADERS)
    resp.raise_for_status()
    data = resp.json()
    if not data.get("success"):
        raise RuntimeError("chatgpt-org-uk: 获取域名失败")
    domains = data.get("data", {}).get("domains", [])
    return [d["domain_name"] for d in domains if d.get("is_active") == 1]


def _create_inbox(email: str) -> tuple[str, str]:
    """创建收件箱，返回 (inbox_token, gm_sid)"""
    resp = tm_http.post(
        f"{BASE_URL}/inbox-token",
        headers={**DEFAULT_HEADERS, "Content-Type": "application/json"},
        json={"email": email},
    )
    resp.raise_for_status()
    gm_sid = _extract_gm_sid(resp)
    data = resp.json()
    token = (data.get("auth") or {}).get("token")
    if not token:
        raise RuntimeError("chatgpt-org-uk: inbox-token 响应缺少 token")
    return token, gm_sid


def generate_email(**kwargs) -> EmailInfo:
    """创建临时邮箱"""
    domains = _fetch_domains()
    if not domains:
        raise RuntimeError("chatgpt-org-uk: 无可用域名")
    domain = random.choice(domains)
    username = _random_username(10)
    email = f"{username}@{domain}"
    inbox_token, gm_sid = _create_inbox(email)
    packed = json.dumps({"gmSid": gm_sid, "inbox": inbox_token}, separators=(",", ":"))

    return EmailInfo(
        channel=CHANNEL,
        email=email,
        _token=packed,
    )


def _parse_packed_token(packed: str) -> tuple[str, str]:
    t = packed.strip()
    if t.startswith("{"):
        try:
            o = json.loads(t)
            gs = o.get("gmSid", "")
            ib = o.get("inbox", "")
            if isinstance(gs, str) and isinstance(ib, str):
                return gs, ib
        except Exception:
            pass
    return "", packed


def _fetch_emails(inbox_token: str, gm_sid: str, email: str) -> list:
    headers = {
        **DEFAULT_HEADERS,
        "x-inbox-token": inbox_token,
    }
    if gm_sid:
        headers["Cookie"] = f"gm_sid={gm_sid}"
    resp = tm_http.get(
        f"{BASE_URL}/emails?email={quote(email)}",
        headers=headers,
    )
    resp.raise_for_status()
    data = resp.json()
    if not data.get("success"):
        raise RuntimeError("chatgpt-org-uk: 获取邮件失败")
    return [
        normalize_email(raw, email)
        for raw in (data.get("data", {}).get("emails") or [])
    ]


def get_emails(token: str, email: str, **kwargs) -> list:
    """获取邮件列表"""
    if not token:
        raise ValueError("chatgpt-org-uk: token 不能为空")

    gm_sid, inbox = _parse_packed_token(token)

    if not gm_sid:
        inbox, gm_sid = _create_inbox(email)

    try:
        return _fetch_emails(inbox, gm_sid, email)
    except Exception as exc:
        if "401" in str(exc) or "403" in str(exc):
            new_inbox, new_sid = _create_inbox(email)
            return _fetch_emails(new_inbox, new_sid, email)
        raise
