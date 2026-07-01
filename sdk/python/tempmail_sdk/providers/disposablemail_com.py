"""
disposablemail.com 临时邮箱渠道（METRONET s.r.o. 后端）

与 fakemail.net / minuteinbox.com 共享相同的 PHP API 结构。
流程：GET / 获取 session cookie + CSRF token → GET /index/index?csrf_token={csrf} 创建邮箱
      GET /index/refresh 获取邮件列表 → GET /email/id/{id} 获取邮件 HTML 正文
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

CHANNEL = "disposablemail-com"
BASE_URL = "https://www.disposablemail.com"
_TOK_PREFIX = "dmc1:"

# 从 HTML 中提取 CSRF token（格式: CSRF="xxx"）
_CSRF_RE = re.compile(r'CSRF="([^"]+)"')

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


def _ajax_headers(cookie_hdr: str) -> Dict[str, str]:
    headers = {
        "User-Agent": _UA,
        "Accept": "application/json, text/javascript, */*; q=0.01",
        "Accept-Language": "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
        "X-Requested-With": "XMLHttpRequest",
        "Referer": f"{BASE_URL}/",
    }
    if cookie_hdr:
        headers["Cookie"] = cookie_hdr
    return headers


def _cookie_header(prev: str, resp: requests.Response) -> str:
    """合并已有 cookie 串与响应新 cookie（按键排序）"""
    d: Dict[str, str] = {}
    for part in (prev or "").split(";"):
        part = part.strip()
        if "=" in part:
            k, _, v = part.partition("=")
            k = k.strip()
            if k:
                d[k] = v.strip()
    d.update(resp.cookies.get_dict())
    return "; ".join(f"{k}={d[k]}" for k in sorted(d.keys()))


def _encode_token(csrf: str, cookie_hdr: str) -> str:
    raw = json.dumps({"t": csrf, "c": cookie_hdr}, separators=(",", ":")).encode("utf-8")
    return _TOK_PREFIX + base64.b64encode(raw).decode("ascii")


def _decode_token(token: str) -> Tuple[str, str]:
    if not token.startswith(_TOK_PREFIX):
        raise ValueError("disposablemail-com: 无效的 token")
    try:
        data = base64.b64decode(token[len(_TOK_PREFIX):])
        o = json.loads(data.decode("utf-8"))
    except (ValueError, UnicodeDecodeError) as e:
        raise ValueError("disposablemail-com: 无效的 token") from e
    csrf = str(o.get("t") or "").strip()
    cookie_hdr = str(o.get("c") or "").strip()
    if not csrf:
        raise ValueError("disposablemail-com: token 中缺少 CSRF")
    return csrf, cookie_hdr


def generate_email(channel: str = CHANNEL) -> EmailInfo:
    """
    创建 disposablemail.com 临时邮箱

    1. GET / 获取 session cookie 和 CSRF token（正则匹配 CSRF="xxx"）
    2. GET /index/index?csrf_token={csrf} 创建邮箱，返回 {"email":"user@domain.com"}
    token 序列化保存 CSRF 与 session cookie。
    """
    resp = tm_http.get(f"{BASE_URL}/", headers=_browser_headers())
    resp.raise_for_status()
    cookie_hdr = _cookie_header("", resp)

    m = _CSRF_RE.search(resp.text)
    if not m or not m.group(1):
        raise RuntimeError("disposablemail-com: 未能从首页提取 CSRF token")
    csrf = m.group(1)

    resp2 = tm_http.get(
        f"{BASE_URL}/index/index",
        headers=_ajax_headers(cookie_hdr),
        params={"csrf_token": csrf},
    )
    resp2.raise_for_status()
    cookie_hdr = _cookie_header(cookie_hdr, resp2)

    data = resp2.json()
    if not isinstance(data, dict):
        raise RuntimeError("disposablemail-com: 创建邮箱响应格式异常")
    email = str(data.get("email") or "").strip()
    if not email or "@" not in email:
        raise RuntimeError(f"disposablemail-com: 获取到的邮箱地址无效: {email!r}")

    return EmailInfo(channel=channel, email=email, _token=_encode_token(csrf, cookie_hdr))


def get_emails(email: str, token: str) -> List[Email]:
    """
    获取 disposablemail.com 邮件列表

    1. GET /index/refresh 获取邮件列表（JSON 数组，空收件箱返回数字 0）
    2. 对每封邮件 GET /email/id/{id} 获取 HTML 正文
    字段（捷克语）: predmet=subject, od=from, id=邮件ID, kdy=when, precteno=read status
    """
    email = (email or "").strip()
    if not email:
        raise ValueError("disposablemail-com: 邮箱地址为空")
    _csrf, cookie_hdr = _decode_token((token or "").strip())

    resp = tm_http.get(f"{BASE_URL}/index/refresh", headers=_ajax_headers(cookie_hdr))
    resp.raise_for_status()

    trimmed = resp.text.strip()
    if trimmed in ("0", "", "[]"):
        return []

    mail_list = resp.json()
    if not isinstance(mail_list, list) or not mail_list:
        return []

    out: List[Email] = []
    for item in mail_list:
        if not isinstance(item, dict):
            continue
        mail_id = str(item.get("id") or "")
        raw = {
            "id": mail_id,
            "from": item.get("od", ""),
            "to": email,
            "subject": item.get("predmet", ""),
            "html": _fetch_body(cookie_hdr, mail_id),
            "date": item.get("kdy", ""),
            "isRead": item.get("precteno") == "precteno",
        }
        out.append(normalize_email(raw, email))

    return out


def _fetch_body(cookie_hdr: str, mail_id: str) -> str:
    """获取单封邮件的 HTML 正文（GET /email/id/{id}），失败返回空串"""
    if not mail_id:
        return ""
    try:
        resp = tm_http.get(f"{BASE_URL}/email/id/{mail_id}", headers=_ajax_headers(cookie_hdr))
    except OSError:
        return ""
    if resp.status_code < 200 or resp.status_code >= 300:
        return ""
    return resp.text
