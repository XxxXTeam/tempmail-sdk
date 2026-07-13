"""
mailmomy 渠道 — https://mailmomy.com

免注册、无鉴权、无 CAPTCHA 的纯 GET JSON API 临时邮箱，Token 即邮箱地址本身。
流程:
  1. GET /api/domains/active → JSON 字符串数组，随机选一个域名（失败回退 mailmomy.com）
  2. 本地随机 10 位 [a-z0-9] local part，拼接为 <local>@<域名>
  3. GET /api/mail/messages?to=<email>&page=1&limit=20 → {"emails": [...], ...}
"""

import random
import string
from typing import Any, Dict, List
from urllib.parse import quote

from ..config import get_config
from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "mailmomy"
BASE_URL = "https://mailmomy.com"
DEFAULT_DOMAIN = "mailmomy.com"


def _get_ua() -> str:
    """获取 User-Agent，优先使用配置中的自定义 UA"""
    c = get_config()
    return (c.headers or {}).get("User-Agent") or (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36"
    )


def _random_local(n: int) -> str:
    """生成 n 位小写字母数字随机邮箱前缀"""
    chars = string.ascii_lowercase + string.digits
    return "".join(random.choice(chars) for _ in range(n))


def _first(msg: Dict[str, Any], *keys: str) -> str:
    """按优先级从消息字典中提取首个非空值并转为字符串"""
    for key in keys:
        val = msg.get(key)
        if val is not None and val != "":
            return str(val)
    return ""


def _pick_domain() -> str:
    """
    拉取 mailmomy 当前可用域名池并随机选取一个
    GET /api/domains/active → JSON 字符串数组；请求或解析失败时回退到默认 mailmomy.com
    """
    try:
        resp = tm_http.get(
            f"{BASE_URL}/api/domains/active",
            headers={"User-Agent": _get_ua(), "Accept": "application/json"},
        )
        resp.raise_for_status()
        data = resp.json()
    except Exception:
        return DEFAULT_DOMAIN

    if not isinstance(data, list):
        return DEFAULT_DOMAIN
    domains = [d for d in data if isinstance(d, str) and d]
    if not domains:
        return DEFAULT_DOMAIN
    return random.choice(domains)


def generate_email() -> EmailInfo:
    """创建 mailmomy.com 临时邮箱；服务免注册，直接构造 <local>@<域名> 即可收信"""
    domain = _pick_domain()
    email = f"{_random_local(10)}@{domain}"
    return EmailInfo(channel=CHANNEL, email=email, _token=email)


def get_emails(email: str) -> List[Email]:
    """
    获取 mailmomy.com 邮件列表
    GET /api/mail/messages?to=<email>&page=1&limit=20
    返回 {"emails": [{id,recipient,from,subject,message,bodyText,receivedAt}], ...}
    """
    addr = (email or "").strip()
    if not addr:
        raise ValueError("mailmomy: 缺少邮箱地址")

    resp = tm_http.get(
        f"{BASE_URL}/api/mail/messages?to={quote(addr, safe='')}&page=1&limit=20",
        headers={"User-Agent": _get_ua(), "Accept": "application/json"},
    )
    resp.raise_for_status()

    data = resp.json()
    if not isinstance(data, dict):
        return []
    emails = data.get("emails")
    if not isinstance(emails, list):
        return []

    out: List[Email] = []
    for msg in emails:
        if not isinstance(msg, dict):
            continue
        message = _first(msg, "message")
        row = {
            "id": _first(msg, "id"),
            "from": _first(msg, "from"),
            "to": _first(msg, "recipient") or addr,
            "subject": _first(msg, "subject"),
            "text": _first(msg, "bodyText") or message,
            "html": message,
            "date": _first(msg, "receivedAt"),
        }
        out.append(normalize_email(row, addr))
    return out
