"""
1SecMail 渠道 -- https://tmaily.com
"""

from typing import Any, Dict, List

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "1sec-mail"
BASE_URL = "https://tmaily.com/"


def _extract_cookie(resp: Any) -> str:
    """从响应中提取 TMaily_sid Cookie"""
    for cookie in resp.cookies:
        if cookie.name == "TMaily_sid":
            return f"TMaily_sid={cookie.value}"
    return ""


def generate_email() -> EmailInfo:
    """生成临时邮箱，从 Set-Cookie 提取会话 Cookie"""
    resp = tm_http.get(
        f"{BASE_URL}generate",
        headers={"Accept": "application/json", "User-Agent": "Mozilla/5.0"},
        timeout=15,
    )
    resp.raise_for_status()
    cookie = _extract_cookie(resp)
    if not cookie:
        raise ValueError("1sec-mail: 未获取到会话 Cookie")
    data = resp.json()
    if not isinstance(data, dict) or not isinstance(data.get("address"), str):
        raise ValueError("1sec-mail: 无效的邮箱响应")
    email = data["address"].strip()
    if "@" not in email:
        raise ValueError("1sec-mail: 无效的邮箱响应")
    return EmailInfo(channel=CHANNEL, email=email, _token=cookie)


def get_emails(token: str, email: str) -> List[Email]:
    """轮询邮件列表，带上会话 Cookie"""
    address = (email or "").strip()
    if not address:
        raise ValueError("1sec-mail: 邮箱地址为空")
    resp = tm_http.get(
        f"{BASE_URL}emails?address={address}",
        headers={
            "Accept": "application/json",
            "Cookie": token,
            "User-Agent": "Mozilla/5.0",
        },
        timeout=15,
    )
    resp.raise_for_status()
    rows = resp.json()
    if not isinstance(rows, list):
        raise ValueError("1sec-mail: 无效的邮件列表响应")
    return [normalize_email(item, address) for item in rows if isinstance(item, dict)]
