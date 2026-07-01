"""
ExpressInboxHub 渠道 — https://expressinboxhub.com
Fake_trash_mail 模式，需要 CSRF token 和 session cookies
域名: mail42.shop
"""

import json
import re
from typing import List

import requests

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "expressinboxhub"
BASE = "https://expressinboxhub.com"

HEADERS = {
    "Accept": "application/json, text/plain, */*",
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
    "Content-Type": "application/json",
    "Origin": BASE,
    "Referer": f"{BASE}/",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
}


def _init_session():
    """
    初始化会话：GET 首页获取 CSRF token 和 session cookies
    返回 (csrf_token, cookie_str)
    """
    session = requests.Session()
    session.headers.update({"User-Agent": HEADERS["User-Agent"]})

    # 应用 SDK 全局配置（代理、SSL 等）
    config_session = tm_http.get_session()
    if config_session.proxies:
        session.proxies = config_session.proxies
    session.verify = config_session.verify

    resp = session.get(BASE, timeout=15)
    resp.raise_for_status()

    # 从 HTML 中提取 <meta name="csrf-token" content="xxx">
    match = re.search(
        r'<meta\s+name=["\']csrf-token["\']\s+content=["\']([^"\']+)["\']',
        resp.text,
    )
    if not match:
        raise RuntimeError("expressinboxhub: 无法提取 CSRF token")

    csrf_token = match.group(1)
    cookie_str = "; ".join(f"{k}={v}" for k, v in session.cookies.items())

    if not cookie_str:
        raise RuntimeError("expressinboxhub: 未获取到 session cookies")

    return csrf_token, cookie_str


def generate_email(channel: str = CHANNEL) -> EmailInfo:
    """创建临时邮箱"""
    csrf_token, cookies = _init_session()

    resp = tm_http.post(
        f"{BASE}/messages",
        headers={
            **HEADERS,
            "X-CSRF-TOKEN": csrf_token,
            "Cookie": cookies,
        },
        json={"_token": csrf_token},
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()

    if not isinstance(data, dict):
        raise RuntimeError("expressinboxhub: 响应格式异常")

    mailbox = data.get("mailbox", "")
    if not mailbox or not str(mailbox).strip():
        raise RuntimeError("expressinboxhub: 响应中缺少 mailbox 字段")

    # 将 CSRF token 和 cookies 序列化存储到 _token 中，获取邮件时需要
    token_payload = json.dumps({"csrfToken": csrf_token, "cookies": cookies})

    return EmailInfo(
        channel=channel,
        email=str(mailbox).strip(),
        _token=token_payload,
    )


def get_emails(token: str, email: str = "") -> List[Email]:
    """获取邮件列表"""
    if not token:
        raise ValueError("expressinboxhub: 缺少 token")

    session_data = json.loads(token)
    csrf_token = session_data["csrfToken"]
    cookies = session_data["cookies"]

    resp = tm_http.post(
        f"{BASE}/messages",
        headers={
            **HEADERS,
            "X-CSRF-TOKEN": csrf_token,
            "Cookie": cookies,
        },
        json={"_token": csrf_token},
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()

    if not isinstance(data, dict):
        return []

    messages = data.get("messages")
    if not isinstance(messages, list):
        return []

    emails: List[Email] = []
    for raw in messages:
        if not isinstance(raw, dict):
            continue
        # 将 receivedAt 映射为 date，content 映射为 html
        normalized = dict(raw)
        if "receivedAt" in normalized and "date" not in normalized:
            normalized["date"] = normalized["receivedAt"]
        if "content" in normalized and "html" not in normalized:
            normalized["html"] = normalized["content"]
        emails.append(normalize_email(normalized, email))

    return emails
