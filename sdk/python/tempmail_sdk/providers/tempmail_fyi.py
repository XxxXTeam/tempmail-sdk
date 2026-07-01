"""
temp-mail.fyi 临时邮箱渠道

流程：GET / 获取 PHPSESSID cookie + 从 HTML 正则提取 csrfToken
      POST /api/generate_email.php 创建邮箱（需 X-CSRF-Token 头 + session cookie）
      POST /api/get_emails.php 获取邮件列表（body 携带 email_address）
token 序列化保存 CSRF token 与 session cookie 串，供 get_emails 复用。
"""

import base64
import json
import re
from typing import Dict, List, Tuple

import requests

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "tempmail-fyi"
BASE_URL = "https://temp-mail.fyi"
_TOK_PREFIX = "tmf1:"

# 从 HTML 中提取 CSRF token（格式: csrfToken" value="xxx"）
_CSRF_RE = re.compile(r'csrfToken"\s*value="([^"]+)"')

_UA = (
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
)


def _browser_headers() -> Dict[str, str]:
    return {
        "User-Agent": _UA,
        "Accept": (
            "text/html,application/xhtml+xml,application/xml;q=0.9,"
            "image/avif,image/webp,image/apng,*/*;q=0.8"
        ),
        "Accept-Language": "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
    }


def _api_headers(csrf: str, cookie_hdr: str) -> Dict[str, str]:
    headers = {
        "User-Agent": _UA,
        "Accept": "application/json, text/javascript, */*; q=0.01",
        "Accept-Language": "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
        "Content-Type": "application/json",
        "X-CSRF-Token": csrf,
        "X-Requested-With": "XMLHttpRequest",
        "Referer": f"{BASE_URL}/",
    }
    if cookie_hdr:
        headers["Cookie"] = cookie_hdr
    return headers


def _cookie_header(resp: requests.Response) -> str:
    d = resp.cookies.get_dict()
    return "; ".join(f"{k}={d[k]}" for k in sorted(d.keys()))


def _encode_token(csrf: str, cookie_hdr: str) -> str:
    raw = json.dumps({"t": csrf, "c": cookie_hdr}, separators=(",", ":")).encode("utf-8")
    return _TOK_PREFIX + base64.b64encode(raw).decode("ascii")


def _decode_token(token: str) -> Tuple[str, str]:
    if not token.startswith(_TOK_PREFIX):
        raise ValueError("tempmail-fyi: 无效的 token")
    try:
        data = base64.b64decode(token[len(_TOK_PREFIX):])
        o = json.loads(data.decode("utf-8"))
    except (ValueError, UnicodeDecodeError) as e:
        raise ValueError("tempmail-fyi: 无效的 token") from e
    csrf = str(o.get("t") or "").strip()
    cookie_hdr = str(o.get("c") or "").strip()
    if not csrf:
        raise ValueError("tempmail-fyi: token 中缺少 CSRF")
    return csrf, cookie_hdr


def generate_email(channel: str = CHANNEL) -> EmailInfo:
    """
    创建 temp-mail.fyi 临时邮箱

    1. GET / 获取 PHPSESSID cookie 和 CSRF token（正则匹配 csrfToken" value="xxx"）
    2. POST /api/generate_email.php 创建邮箱
       返回 {"success":true,"email_address":"xxx@...","expires_at":"...","error":null}
    token 序列化保存 CSRF 与 session cookie。
    """
    resp = tm_http.get(f"{BASE_URL}/", headers=_browser_headers())
    resp.raise_for_status()

    cookie_hdr = _cookie_header(resp)

    m = _CSRF_RE.search(resp.text)
    if not m or not m.group(1):
        raise RuntimeError("tempmail-fyi: 未能从首页提取 CSRF token")
    csrf = m.group(1)

    resp2 = tm_http.post(
        f"{BASE_URL}/api/generate_email.php",
        headers=_api_headers(csrf, cookie_hdr),
        data="{}",
    )
    resp2.raise_for_status()

    data = resp2.json()
    if not isinstance(data, dict):
        raise RuntimeError("tempmail-fyi: 创建邮箱响应格式异常")
    if not data.get("success"):
        err = str(data.get("error") or "").strip()
        raise RuntimeError(f"tempmail-fyi: 创建邮箱失败{'：' + err if err else '（success=false）'}")

    email = str(data.get("email_address") or "").strip()
    if not email or "@" not in email:
        raise RuntimeError(f"tempmail-fyi: 获取到的邮箱地址无效: {email!r}")

    return EmailInfo(
        channel=channel,
        email=email,
        _token=_encode_token(csrf, cookie_hdr),
        expires_at=data.get("expires_at"),
    )


def get_emails(email: str, token: str) -> List[Email]:
    """
    获取 temp-mail.fyi 邮件列表

    POST /api/get_emails.php，body 携带 {"email_address":"xxx@..."}，
    需 X-CSRF-Token 头 + session cookie。返回 {"success":true,"emails":[...],"error":null}
    """
    email = (email or "").strip()
    if not email:
        raise ValueError("tempmail-fyi: 邮箱地址为空")
    csrf, cookie_hdr = _decode_token((token or "").strip())

    resp = tm_http.post(
        f"{BASE_URL}/api/get_emails.php",
        headers=_api_headers(csrf, cookie_hdr),
        data=json.dumps({"email_address": email}),
    )
    resp.raise_for_status()

    data = resp.json()
    if not isinstance(data, dict):
        raise RuntimeError("tempmail-fyi: 邮件列表响应格式异常")
    if not data.get("success"):
        err = str(data.get("error") or "").strip()
        raise RuntimeError(f"tempmail-fyi: 获取邮件列表失败{'：' + err if err else '（success=false）'}")

    emails = data.get("emails")
    if not isinstance(emails, list) or not emails:
        return []

    out: List[Email] = []
    for item in emails:
        if not isinstance(item, dict):
            continue
        out.append(normalize_email(item, email))
    return out
