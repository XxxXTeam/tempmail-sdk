"""
tmail.link 临时邮箱渠道 — https://tmail.link

创建邮箱:
  1. GET / 获取首页 HTML，正则提取邮箱地址（形如 xxx@tmail.link）
  2. GET /inbox/{email}/ 触发下发 Set-Cookie: csrftoken=xxx
  token 存储 csrftoken 值（Django CSRF 令牌），后续请求携带。
获取邮件:
  POST /inbox/{email}/  form: format=json + csrfmiddlewaretoken={token}
  请求头 Cookie: csrftoken={token}
  响应: {"messages": [{key, sender, subject, date}]}
"""

import re
from typing import Dict, List

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "tmail-link"
BASE_URL = "https://tmail.link"

# 从首页 HTML 中提取 tmail.link 邮箱地址
_EMAIL_RE = re.compile(r"([a-zA-Z0-9._%+-]+@tmail\.link)")

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
        "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
    }


def _post_headers(email: str, token: str) -> Dict[str, str]:
    return {
        "User-Agent": _UA,
        "Accept": "application/json, text/javascript, */*; q=0.01",
        "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
        "X-Requested-With": "XMLHttpRequest",
        "Content-Type": "application/x-www-form-urlencoded; charset=UTF-8",
        "Origin": BASE_URL,
        "Referer": f"{BASE_URL}/inbox/{email}/",
        "Cookie": f"csrftoken={token}",
    }


def generate_email(channel: str = CHANNEL) -> EmailInfo:
    """
    创建 tmail.link 临时邮箱

    1. GET / 获取首页 HTML，正则提取邮箱地址
    2. GET /inbox/{email}/ 获取 Set-Cookie 中的 csrftoken
    token 存储 csrftoken 值。
    """
    resp = tm_http.get(f"{BASE_URL}/", headers=_browser_headers())
    resp.raise_for_status()

    m = _EMAIL_RE.search(resp.text)
    if not m:
        raise RuntimeError("tmail-link: 未能从首页提取邮箱地址")
    email = m.group(1).strip()
    if not email:
        raise RuntimeError("tmail-link: 提取的邮箱地址为空")

    resp2 = tm_http.get(f"{BASE_URL}/inbox/{email}/", headers=_browser_headers())
    resp2.raise_for_status()

    token = resp2.cookies.get("csrftoken", "")
    if not token:
        raise RuntimeError("tmail-link: 未能获取 csrftoken")

    return EmailInfo(channel=channel, email=email, _token=token)


def get_emails(email: str, token: str) -> List[Email]:
    """
    获取 tmail.link 邮件列表

    POST /inbox/{email}/  form: format=json + csrfmiddlewaretoken={token}
    响应: {"messages": [{key, sender, subject, date}]}
    字段映射: key=id, sender=from, subject=subject, date=date。
    """
    email = (email or "").strip()
    if not email:
        raise ValueError("tmail-link: 邮箱地址为空")

    resp = tm_http.post(
        f"{BASE_URL}/inbox/{email}/",
        headers=_post_headers(email, token),
        data={"format": "json", "csrfmiddlewaretoken": token},
    )
    resp.raise_for_status()

    result = resp.json()
    if not isinstance(result, dict):
        raise RuntimeError("tmail-link: 邮件列表响应格式异常")

    messages = result.get("messages")
    if not isinstance(messages, list) or not messages:
        return []

    out: List[Email] = []
    for item in messages:
        if not isinstance(item, dict):
            continue
        raw = {
            "id": item.get("key", ""),
            "from": item.get("sender", ""),
            "to": email,
            "subject": item.get("subject", ""),
            "date": item.get("date", ""),
        }
        out.append(normalize_email(raw, email))
    return out
