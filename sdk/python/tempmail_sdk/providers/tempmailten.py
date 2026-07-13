"""
tempmailten.com 临时邮箱渠道（Laravel + CSRF）

流程：GET /en 获取 session cookie + 从 HTML meta 提取 CSRF token
      POST /messages（body: _token={csrf}&captcha=）获取邮箱地址和邮件列表
      GET /view/{id} 获取单封邮件 HTML 正文
该站点 reCAPTCHA 已禁用，captcha 参数传空即可。
token 序列化保存 CSRF token 与 session cookie 串，供 get_emails 复用（参考 moakt 模式）。
"""

import base64
import json
import re
from typing import Dict, List, Tuple

import requests

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "tempmailten"
BASE_URL = "https://tempmailten.com"
_TOK_PREFIX = "tmt1:"

# 从 meta 标签提取 CSRF token（<meta name="csrf-token" content="xxx">）
_CSRF_RE = re.compile(r'<meta\s+name="csrf-token"\s+content="([^"]+)"')

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


def _ajax_headers() -> Dict[str, str]:
    return {
        "User-Agent": _UA,
        "Accept": "application/json, text/javascript, */*; q=0.01",
        "Accept-Language": "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
        "X-Requested-With": "XMLHttpRequest",
        "Referer": f"{BASE_URL}/en",
    }


def _cookie_header(resp: requests.Response) -> str:
    d = resp.cookies.get_dict()
    return "; ".join(f"{k}={d[k]}" for k in sorted(d.keys()))


def _encode_token(csrf: str, cookie_hdr: str) -> str:
    raw = json.dumps({"t": csrf, "c": cookie_hdr}, separators=(",", ":")).encode(
        "utf-8"
    )
    return _TOK_PREFIX + base64.b64encode(raw).decode("ascii")


def _decode_token(token: str) -> Tuple[str, str]:
    if not token.startswith(_TOK_PREFIX):
        raise ValueError("tempmailten: 无效的 token")
    try:
        data = base64.b64decode(token[len(_TOK_PREFIX) :])
        o = json.loads(data.decode("utf-8"))
    except (ValueError, UnicodeDecodeError) as e:
        raise ValueError("tempmailten: 无效的 token") from e
    csrf = str(o.get("t") or "").strip()
    cookie_hdr = str(o.get("c") or "").strip()
    if not csrf:
        raise ValueError("tempmailten: token 中缺少 CSRF")
    return csrf, cookie_hdr


def _post_messages(csrf: str, cookie_hdr: str) -> dict:
    """POST /messages 获取 {mailbox, messages} JSON"""
    headers = _ajax_headers()
    headers["Content-Type"] = "application/x-www-form-urlencoded"
    headers["X-CSRF-TOKEN"] = csrf
    if cookie_hdr:
        headers["Cookie"] = cookie_hdr

    resp = tm_http.post(
        f"{BASE_URL}/messages",
        headers=headers,
        data={"_token": csrf, "captcha": ""},
    )
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, dict):
        raise RuntimeError("tempmailten: 解析响应 JSON 失败")
    return data


def generate_email(channel: str = CHANNEL) -> EmailInfo:
    """
    创建 tempmailten.com 临时邮箱

    1. GET /en 获取 session cookie 和 CSRF token（正则匹配 meta csrf-token）
    2. POST /messages（body: _token={csrf}&captcha=）获取分配的邮箱地址
    token 序列化保存 CSRF 与 session cookie。
    """
    resp = tm_http.get(f"{BASE_URL}/en", headers=_browser_headers())
    resp.raise_for_status()

    cookie_hdr = _cookie_header(resp)

    m = _CSRF_RE.search(resp.text)
    if not m:
        raise RuntimeError("tempmailten: 未能从首页提取 CSRF token")
    csrf = m.group(1)

    data = _post_messages(csrf, cookie_hdr)
    mailbox = str(data.get("mailbox") or "").strip()
    if not mailbox or "@" not in mailbox:
        raise RuntimeError(f"tempmailten: 邮箱地址无效: {mailbox!r}")

    return EmailInfo(
        channel=channel, email=mailbox, _token=_encode_token(csrf, cookie_hdr)
    )


def _fetch_view(cookie_hdr: str, mid: str) -> str:
    """获取单封邮件的 HTML 正文（GET /view/{id}），失败返回空串"""
    if not mid:
        return ""
    from urllib.parse import quote

    headers = _browser_headers()
    headers["Referer"] = f"{BASE_URL}/en"
    if cookie_hdr:
        headers["Cookie"] = cookie_hdr
    try:
        resp = tm_http.get(f"{BASE_URL}/view/{quote(mid, safe='')}", headers=headers)
    except OSError:
        return ""
    if resp.status_code < 200 or resp.status_code >= 300:
        return ""
    return resp.text


def get_emails(email: str, token: str) -> List[Email]:
    """
    获取 tempmailten.com 邮件列表

    1. POST /messages 获取 {mailbox, messages} JSON
    2. 对每封邮件 GET /view/{id} 获取 HTML 正文
    messages 数组元素: {id, from, from_email, subject, is_seen}
    """
    email = (email or "").strip()
    if not email:
        raise ValueError("tempmailten: 邮箱地址为空")
    csrf, cookie_hdr = _decode_token((token or "").strip())

    data = _post_messages(csrf, cookie_hdr)
    msgs = data.get("messages")
    if not isinstance(msgs, list) or not msgs:
        return []

    out: List[Email] = []
    for msg in msgs:
        if not isinstance(msg, dict):
            continue
        mid = str(msg.get("id") or "").strip()
        if not mid:
            continue

        from_addr = str(msg.get("from_email") or "")
        from_name = str(msg.get("from") or "")
        if from_name and from_name != from_addr:
            from_addr = f"{from_name} <{from_addr}>"

        raw = {
            "id": mid,
            "from": from_addr,
            "to": email,
            "subject": msg.get("subject", ""),
            "html": _fetch_view(cookie_hdr, mid),
            "isRead": bool(msg.get("is_seen")),
        }
        out.append(normalize_email(raw, email))

    return out
