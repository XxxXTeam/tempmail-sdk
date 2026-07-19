"""
TempGmailer 渠道实现
网站: https://tempgmailer.com

流程：
  1. GET 首页获取 CSRF token 和 Laravel session cookie
  2. POST /get-gmail 创建临时 Gmail 邮箱
  3. POST /get-inbox 获取收件箱
"""

import json
import re
from typing import List, Optional

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "tempgmailer"
BASE_URL = "https://tempgmailer.com"

_HEADERS = {
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
    "Accept": "application/json, text/plain, */*",
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
    "Referer": f"{BASE_URL}/",
    "X-Requested-With": "XMLHttpRequest",
    "X-TempGmailer-Auth": "frontend",
    "Content-Type": "application/json",
}


def _init_session() -> tuple:
    """
    访问首页获取 CSRF token 和 session cookie

    Returns:
        (csrf_token, cookie_str)
    """
    import requests

    session = requests.Session()
    session.headers.update({"User-Agent": _HEADERS["User-Agent"]})
    resp = session.get(BASE_URL, timeout=15)
    resp.raise_for_status()

    # 从 HTML 中提取 CSRF token
    match = re.search(
        r'<meta\s+name="csrf-token"\s+content="([^"]+)"', resp.text
    )
    if not match:
        raise RuntimeError("tempgmailer: 无法提取 CSRF token")
    csrf_token = match.group(1)

    # 组装 cookie 字符串
    cookie_str = "; ".join([f"{k}={v}" for k, v in session.cookies.items()])
    if not cookie_str:
        raise RuntimeError("tempgmailer: 未获取到 session cookie")

    return csrf_token, cookie_str


def generate_email(
    duration: int = 30, domain: Optional[str] = None
) -> EmailInfo:
    """
    创建 TempGmailer 临时邮箱

    Args:
        duration: 有效期（分钟），本渠道不使用此参数
        domain: 域名筛选，本渠道不使用此参数

    Returns:
        EmailInfo 对象
    """
    csrf_token, cookies = _init_session()

    headers = {
        **_HEADERS,
        "X-CSRF-TOKEN": csrf_token,
        "Cookie": cookies,
    }

    resp = tm_http.post(
        f"{BASE_URL}/get-gmail",
        headers=headers,
        json={"refresh": True, "adblock": 0},
    )
    resp.raise_for_status()
    data = resp.json()

    if not data.get("success"):
        raise RuntimeError(
            f"tempgmailer: 创建邮箱失败: {data.get('message', '未知错误')}"
        )

    email = (data.get("data") or {}).get("email", "").strip()
    if not email:
        raise RuntimeError("tempgmailer: 创建邮箱失败, 未返回邮箱地址")

    # 将 CSRF token 和 cookie 组合为 JSON 字符串作为 token
    token_payload = json.dumps({"csrfToken": csrf_token, "cookies": cookies})

    return EmailInfo(
        channel=CHANNEL,
        email=email,
        _token=token_payload,
    )


def get_emails(token: str, email: str = "", **kwargs) -> List[Email]:
    """
    获取 TempGmailer 收件箱邮件

    Args:
        token: JSON 格式的认证信息（csrf_token + cookies）
        email: 邮箱地址

    Returns:
        标准化邮件列表
    """
    session = json.loads(token)
    csrf_token = session["csrfToken"]
    cookies = session["cookies"]

    headers = {
        **_HEADERS,
        "X-CSRF-TOKEN": csrf_token,
        "Cookie": cookies,
    }

    resp = tm_http.post(
        f"{BASE_URL}/get-inbox",
        headers=headers,
        json={"email": email, "adblock": 0},
    )
    resp.raise_for_status()
    data = resp.json()

    if not data.get("success"):
        return []

    messages = (data.get("data") or {}).get("messages", [])
    if not isinstance(messages, list):
        return []

    emails: List[Email] = []
    for msg in messages:
        if not isinstance(msg, dict):
            continue

        # from 字段可能是对象 {"address": "..."} 或字符串
        from_field = msg.get("from", "")
        if isinstance(from_field, dict):
            from_field = from_field.get("address", "")

        # body 为完整 HTML，intro 为纯文本摘要
        html_content = msg.get("body") or msg.get("intro", "")
        text_content = msg.get("intro", "")

        raw = {
            "id": msg.get("id", ""),
            "from": from_field,
            "to": email,
            "subject": msg.get("subject", ""),
            "text": text_content,
            "html": html_content,
            "date": msg.get("createdAt", ""),
        }
        emails.append(normalize_email(raw, email))

    return emails
