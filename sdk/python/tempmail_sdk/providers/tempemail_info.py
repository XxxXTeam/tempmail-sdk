"""
tempemail.info 临时邮箱渠道

流程：GET / 获取 PHPSESSID 会话 Cookie，并从 HTML 中解析 base64 编码的邮箱地址
      POST /template/checker.php（body: last_id=0）获取邮件列表（JSON 数组）
      GET /view/{date} 获取单封邮件 HTML 正文
token 存储会话 Cookie 串，用于后续请求绑定收件箱。
"""

import base64
import re
from typing import Dict, List
from urllib.parse import quote

import requests

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "tempemail-info"
BASE_URL = "https://tempemail.info"

# 匹配首页 HTML 中的 var emailEncoded = "base64..."
_EMAIL_RE = re.compile(r'var\s+emailEncoded\s*=\s*"([^"]+)"')


def _base_headers() -> Dict[str, str]:
    return {
        "User-Agent": (
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
            "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
        ),
        "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
        "Referer": f"{BASE_URL}/",
        "Origin": BASE_URL,
    }


def _cookie_header(resp: requests.Response) -> str:
    """将响应 Set-Cookie 合并为 Cookie 请求头串（按键排序）"""
    d = resp.cookies.get_dict()
    return "; ".join(f"{k}={d[k]}" for k in sorted(d.keys()))


def generate_email(channel: str = CHANNEL) -> EmailInfo:
    """
    创建 tempemail.info 临时邮箱

    1. GET / 获取 PHPSESSID 会话 Cookie
    2. 从 HTML 正则提取 emailEncoded 并 base64 解码得到邮箱地址
    token 存储会话 Cookie 串，供后续 checker.php / view 请求使用。
    """
    headers = _base_headers()
    headers["Accept"] = (
        "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"
    )

    resp = tm_http.get(f"{BASE_URL}/", headers=headers)
    resp.raise_for_status()

    cookie_hdr = _cookie_header(resp)
    if not cookie_hdr:
        raise RuntimeError("tempemail-info: 未获取到会话 Cookie")

    m = _EMAIL_RE.search(resp.text)
    if not m:
        raise RuntimeError("tempemail-info: 未在页面中找到 emailEncoded")
    try:
        decoded = base64.b64decode(m.group(1)).decode("utf-8", "replace").strip()
    except (ValueError, UnicodeDecodeError) as e:
        raise RuntimeError("tempemail-info: 邮箱地址 base64 解码失败") from e
    if not decoded or "@" not in decoded:
        raise RuntimeError("tempemail-info: 解码出的邮箱地址无效")

    return EmailInfo(channel=channel, email=decoded, _token=cookie_hdr)


def _fetch_body(cookie_hdr: str, date: str) -> str:
    """获取单封邮件的 HTML 正文（GET /view/{date}），失败返回空串"""
    if not (date or "").strip():
        return ""
    headers = _base_headers()
    headers["Accept"] = (
        "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"
    )
    headers["Cookie"] = cookie_hdr
    try:
        resp = tm_http.get(f"{BASE_URL}/view/{quote(date, safe='')}", headers=headers)
    except OSError:
        return ""
    if resp.status_code < 200 or resp.status_code >= 300:
        return ""
    return resp.text


def get_emails(email: str, token: str) -> List[Email]:
    """
    获取 tempemail.info 邮件列表

    1. POST /template/checker.php（body: last_id=0）获取邮件列表 JSON 数组
    2. 对每封邮件 GET /view/{date} 获取 HTML 正文
    checker.php 返回字段：id / name / from / subject / date / read，
    其中 date 既是显示日期，又是 /view/{date} 正文接口的路径键。
    """
    cookie_hdr = (token or "").strip()
    if not cookie_hdr:
        raise ValueError("tempemail-info: 缺少会话 Cookie")

    headers = _base_headers()
    headers["Accept"] = "application/json, text/javascript, */*; q=0.01"
    headers["X-Requested-With"] = "XMLHttpRequest"
    headers["Content-Type"] = "application/x-www-form-urlencoded"
    headers["Cookie"] = cookie_hdr

    resp = tm_http.post(
        f"{BASE_URL}/template/checker.php",
        headers=headers,
        data={"last_id": "0"},
    )
    resp.raise_for_status()

    rows = resp.json()
    if not isinstance(rows, list) or not rows:
        return []

    out: List[Email] = []
    for row in rows:
        if not isinstance(row, dict):
            continue
        # 构造发件人地址：有显示名则格式化为 "name <email>"
        from_addr = str(row.get("from") or "")
        name = str(row.get("name") or "")
        if name and name != from_addr:
            from_addr = f"{name} <{from_addr}>"

        date = str(row.get("date") or "")
        raw = {
            "id": str(row.get("id") or ""),
            "from": from_addr,
            "to": email,
            "subject": row.get("subject", ""),
            "date": date,
            "html": _fetch_body(cookie_hdr, date),
            "isRead": bool(row.get("read")),
        }
        out.append(normalize_email(raw, email))

    return out
