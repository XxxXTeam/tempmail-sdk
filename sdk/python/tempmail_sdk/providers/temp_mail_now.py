"""
Temp-Mail.Now 渠道 — https://temp-mail.now
基于会话 Cookie 的临时邮箱服务。
获取会话: GET /en/ 获取 session cookie
创建邮箱: POST /change_email 使用 session cookie，返回 {"new_email":"xxx@dpmurt.my"}
获取邮件: GET /fetch_emails 使用 session cookie，返回 {"emails":[...],"remaining_time":n}
"""

import time
from typing import Any, Dict, List

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "temp-mail-now"
BASE_URL = "https://temp-mail.now"

# 首页请求头
_PAGE_HEADERS = {
    "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
}

# API 请求头
_API_HEADERS = {
    "Accept": "application/json, text/javascript, */*; q=0.01",
    "X-Requested-With": "XMLHttpRequest",
    "Referer": f"{BASE_URL}/en/",
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
}


def _merge_cookies(resp: Any) -> str:
    """从响应中提取并合并 Cookie 字符串"""
    jar: Dict[str, str] = {}
    for cookie in resp.cookies:
        jar[cookie.name] = cookie.value
    return "; ".join(f"{k}={v}" for k, v in jar.items())


def _api_headers_with_cookie(cookie: str) -> Dict[str, str]:
    """生成带 Cookie 的 API 请求头"""
    headers = dict(_API_HEADERS)
    headers["Cookie"] = cookie
    return headers


def generate_email(channel: str = CHANNEL) -> EmailInfo:
    """
    创建 temp-mail.now 临时邮箱

    流程:
    1. GET /en/ 获取 session cookie
    2. POST /change_email 使用 session cookie 创建新邮箱
    3. 响应: {"new_email":"xxx@dpmurt.my"}
    4. token 存储 session cookie，后续获取邮件时使用
    """
    # 获取 session cookie
    resp = tm_http.get(f"{BASE_URL}/en/", headers=_PAGE_HEADERS, timeout=15)
    resp.raise_for_status()
    cookie = _merge_cookies(resp)
    if not cookie:
        raise RuntimeError("temp-mail-now: 无法获取 session cookie")

    # 创建新邮箱
    resp2 = tm_http.post(
        f"{BASE_URL}/change_email",
        headers=_api_headers_with_cookie(cookie),
        timeout=15,
    )
    resp2.raise_for_status()

    # 合并可能更新的 cookie
    for c in resp2.cookies:
        parts = {
            k: v
            for k, v in (
                p.strip().split("=", 1) for p in cookie.split(";") if "=" in p.strip()
            )
        }
        parts[c.name] = c.value
        cookie = "; ".join(f"{k}={v}" for k, v in parts.items())

    data = resp2.json()
    if not isinstance(data, dict):
        raise RuntimeError("temp-mail-now: 响应格式无效")

    address = data.get("new_email", "")
    if not address or "@" not in address:
        raise RuntimeError(f"temp-mail-now: 创建邮箱失败，响应: {data!r}")

    return EmailInfo(
        channel=channel,
        email=address,
        _token=cookie,
    )


def get_emails(token: str, email: str) -> List[Email]:
    """
    获取 temp-mail.now 邮件列表

    流程:
    1. GET /fetch_emails 使用 session cookie
    2. 响应格式: {"emails":[...],"remaining_time":n}
    3. 遍历 emails 数组，使用 normalize_email 标准化
    """
    if not token:
        raise ValueError("temp-mail-now: session cookie 不能为空")

    addr = (email or "").strip()
    if not addr:
        raise ValueError("temp-mail-now: 邮箱地址不能为空")

    resp = tm_http.get(
        f"{BASE_URL}/fetch_emails",
        headers=_api_headers_with_cookie(token),
        timeout=15,
    )
    resp.raise_for_status()

    data = resp.json()
    if not isinstance(data, dict):
        return []

    emails_list = data.get("emails")
    if not isinstance(emails_list, list):
        return []

    out: List[Email] = []
    for item in emails_list:
        if not isinstance(item, dict):
            continue
        raw = {
            "id": item.get("id", ""),
            "from": item.get("from", item.get("from_address", item.get("sender", ""))),
            "to": item.get("to", addr),
            "subject": item.get("subject", ""),
            "text": item.get("text", item.get("body_text", "")),
            "html": item.get("html", item.get("body_html", "")),
            "date": item.get("date", item.get("received_at", "")),
        }
        out.append(normalize_email(raw, addr))

    return out
