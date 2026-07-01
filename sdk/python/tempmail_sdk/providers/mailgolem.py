"""
MailGolem 渠道 — https://mailgolem.com
基于 Laravel session + CSRF token 的临时邮箱服务。
创建邮箱: GET / 获取 session cookie 和 CSRF token，GET /random-email-address 获取邮箱地址。
获取邮件: GET / 获取新 session + CSRF，POST /fetch-emails/{email} 获取邮件列表。
"""

import base64
import json
import re
from typing import Dict, List, Optional

import requests

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "mailgolem"
BASE = "https://mailgolem.com"

# 从 HTML 中提取 CSRF token: <input type="hidden" name="_token" id="token" value="{csrf_token}">
_CSRF_RE = re.compile(r'<input[^>]+name="_token"[^>]+value="([^"]+)"')

# token 前缀
_TOK_PREFIX = "mgolem1:"

# 默认请求头
_DEFAULT_HEADERS = {
    "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
    "Accept-Language": "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
    "Cache-Control": "no-cache",
    "DNT": "1",
    "Pragma": "no-cache",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
}


def _build_cookie_header(cookies: dict) -> str:
    """将 cookies 字典构建为 Cookie 请求头字符串"""
    return "; ".join(f"{k}={v}" for k, v in cookies.items())


def _merge_cookies(existing: dict, response: requests.Response) -> dict:
    """合并响应中的 Set-Cookie 到现有 cookies"""
    merged = dict(existing)
    merged.update(response.cookies.get_dict())
    return merged


def _extract_csrf(html_text: str) -> str:
    """从 HTML 页面中提取 CSRF token"""
    m = _CSRF_RE.search(html_text)
    if not m:
        raise RuntimeError("mailgolem: 无法从页面中提取 CSRF token")
    return m.group(1)


def _encode_token(csrf: str, cookies: dict) -> str:
    """将 CSRF token 和 cookies 编码为 SDK token 字符串"""
    payload = json.dumps({"csrf": csrf, "c": cookies}, separators=(",", ":"))
    encoded = base64.standard_b64encode(payload.encode("utf-8")).decode("ascii")
    return _TOK_PREFIX + encoded


def _decode_token(token: str) -> tuple:
    """从 SDK token 中解码出 CSRF token 和 cookies"""
    if not token or not token.startswith(_TOK_PREFIX):
        raise ValueError("mailgolem: 无效的 session token")
    try:
        raw = base64.standard_b64decode(token[len(_TOK_PREFIX):].encode("ascii"))
        data = json.loads(raw.decode("utf-8"))
    except (json.JSONDecodeError, ValueError) as e:
        raise ValueError("mailgolem: 无效的 session token") from e
    csrf = data.get("csrf", "")
    cookies = data.get("c", {})
    if not csrf or not isinstance(cookies, dict):
        raise ValueError("mailgolem: 无效的 session token (缺少 csrf 或 cookies)")
    return csrf, cookies


def generate_email(channel: str = CHANNEL) -> EmailInfo:
    """
    创建 mailgolem 临时邮箱

    流程:
    1. GET / 获取 session cookie 和 CSRF token
    2. GET /random-email-address 获取随机邮箱地址
    """
    # 第一步: GET 首页，建立 session 并提取 CSRF token
    r1 = tm_http.get(
        f"{BASE}/",
        headers={**_DEFAULT_HEADERS},
    )
    r1.raise_for_status()
    cookies = r1.cookies.get_dict()
    csrf = _extract_csrf(r1.text)

    # 第二步: GET /random-email-address 获取邮箱地址
    r2 = tm_http.get(
        f"{BASE}/random-email-address",
        headers={
            **_DEFAULT_HEADERS,
            "Referer": f"{BASE}/",
            "Cookie": _build_cookie_header(cookies),
        },
    )
    r2.raise_for_status()
    cookies = _merge_cookies(cookies, r2)

    email_addr = r2.text.strip()
    if not email_addr or "@" not in email_addr:
        raise RuntimeError(f"mailgolem: 获取到无效的邮箱地址: {email_addr!r}")

    token = _encode_token(csrf, cookies)
    return EmailInfo(channel=channel, email=email_addr, _token=token)


def get_emails(token: str, email: str = "") -> List[Email]:
    """
    获取 mailgolem 邮件列表

    流程:
    1. GET / 获取新 session 和 CSRF token（session 可能已过期）
    2. POST /fetch-emails/{email} 获取邮件列表
    """
    if not token:
        raise ValueError("mailgolem: token 不能为空")
    if not (email or "").strip():
        raise ValueError("mailgolem: 邮箱地址不能为空")

    addr = email.strip()

    # 重新获取 session 和 CSRF（原 session 可能已过期）
    r1 = tm_http.get(
        f"{BASE}/",
        headers={**_DEFAULT_HEADERS},
    )
    r1.raise_for_status()
    cookies = r1.cookies.get_dict()
    new_csrf = _extract_csrf(r1.text)

    # POST /fetch-emails/{email} 获取邮件列表
    r2 = tm_http.post(
        f"{BASE}/fetch-emails/{addr}",
        headers={
            "Accept": "application/json, text/plain, */*",
            "Accept-Language": "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
            "Content-Type": "application/x-www-form-urlencoded",
            "X-Requested-With": "XMLHttpRequest",
            "Referer": f"{BASE}/",
            "User-Agent": _DEFAULT_HEADERS["User-Agent"],
            "Cookie": _build_cookie_header(cookies),
        },
        data=f"_token={new_csrf}",
    )
    r2.raise_for_status()

    body = r2.json()
    if not isinstance(body, list):
        return []

    out: List[Email] = []
    for item in body:
        if not isinstance(item, dict):
            continue
        raw = {
            "id": str(item.get("id", "")),
            "from": item.get("from", ""),
            "to": addr,
            "subject": item.get("subject", ""),
        }
        out.append(normalize_email(raw, addr))

    return out
