"""
linshiyouxiang.net 临时邮箱渠道 — https://www.linshiyouxiang.net

创建邮箱: GET / 获取首页 HTML，正则提取 tempMailGlobal（邮箱）和 mailCodeGlobal（校验 code）
获取邮件: POST /get-messages  body: {"email":"...","code":"..."}
token 存储 mailCodeGlobal 的值（HMAC 哈希，后续请求用于校验，不依赖 cookie）。
"""

import re
from typing import Dict, List

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "linshiyouxiang-net"
BASE_URL = "https://www.linshiyouxiang.net"

# 提取首页 HTML 中的 tempMailGlobal（邮箱地址）
_EMAIL_RE = re.compile(r"tempMailGlobal\s*=\s*'([^']+)'")
# 提取首页 HTML 中的 mailCodeGlobal（校验 code）
_CODE_RE = re.compile(r"mailCodeGlobal\s*=\s*'([^']+)'")

_UA = (
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
)


def _api_headers() -> Dict[str, str]:
    return {
        "Accept": "application/json, text/javascript, */*; q=0.01",
        "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
        "Referer": f"{BASE_URL}/",
        "Origin": BASE_URL,
        "User-Agent": _UA,
        "Content-Type": "application/json",
        "X-Requested-With": "XMLHttpRequest",
    }


def generate_email(channel: str = CHANNEL) -> EmailInfo:
    """
    创建 linshiyouxiang.net 临时邮箱

    1. GET / 获取首页 HTML
    2. 从 HTML 正则提取 tempMailGlobal（邮箱）和 mailCodeGlobal（校验 code）
    token 存储 mailCodeGlobal。
    """
    headers = {
        "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
        "User-Agent": _UA,
    }
    resp = tm_http.get(f"{BASE_URL}/", headers=headers)
    resp.raise_for_status()
    html = resp.text

    m = _EMAIL_RE.search(html)
    if not m:
        raise RuntimeError("linshiyouxiang-net: 未能从首页提取邮箱地址")
    email = m.group(1).strip()
    if not email:
        raise RuntimeError("linshiyouxiang-net: 提取的邮箱地址为空")

    code_match = _CODE_RE.search(html)
    code = code_match.group(1).strip() if code_match else ""

    return EmailInfo(channel=channel, email=email, _token=code)


def get_emails(email: str, token: str) -> List[Email]:
    """
    获取 linshiyouxiang.net 邮件列表

    POST /get-messages  body: {"email":"...","code":"<token>"}
    响应: {"emails":null|[...],"success":true}
    """
    email = (email or "").strip()
    if not email:
        raise ValueError("linshiyouxiang-net: 邮箱地址为空")

    resp = tm_http.post(
        f"{BASE_URL}/get-messages",
        headers=_api_headers(),
        json={"email": email, "code": token or ""},
    )
    resp.raise_for_status()

    result = resp.json()
    if not isinstance(result, dict):
        raise RuntimeError("linshiyouxiang-net: 邮件列表响应格式异常")

    emails = result.get("emails")
    if not isinstance(emails, list) or not emails:
        return []

    out: List[Email] = []
    for item in emails:
        if not isinstance(item, dict):
            continue
        out.append(normalize_email(item, email))
    return out
