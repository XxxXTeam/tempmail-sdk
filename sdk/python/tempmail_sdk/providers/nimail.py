"""
nimail 渠道 — https://www.nimail.cn

简单 POST 表单 API 临时邮箱，无需认证，Token 即邮箱地址本身。
流程:
  1. POST /api/applymail (mail={前缀}@nimail.cn) → 注册邮箱，返回 {"user": "xxx@nimail.cn", "success": "true"}
  2. POST /api/getmails (mail={email}&time=0) → 返回 {"mail": [...], "success": "true"}
"""

import random
import string
from typing import Any, Dict, List
from urllib.parse import quote

from ..config import get_config
from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "nimail"
BASE_URL = "https://www.nimail.cn"


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


def generate_email() -> EmailInfo:
    """创建 nimail.cn 临时邮箱"""
    email = f"{_random_local(10)}@nimail.cn"

    resp = tm_http.post(
        f"{BASE_URL}/api/applymail",
        headers={
            "User-Agent": _get_ua(),
            "Content-Type": "application/x-www-form-urlencoded",
            "Origin": BASE_URL,
            "Referer": f"{BASE_URL}/",
        },
        data=f"mail={quote(email, safe='')}",
    )
    resp.raise_for_status()

    data = resp.json()
    if (
        not isinstance(data, dict)
        or data.get("success") != "true"
        or not data.get("user")
    ):
        raise RuntimeError(f"nimail: 创建邮箱失败 {data!r}")

    user = str(data["user"])
    return EmailInfo(channel=CHANNEL, email=user, _token=user)


def get_emails(email: str) -> List[Email]:
    """获取 nimail.cn 邮件列表"""
    addr = (email or "").strip()
    if not addr:
        raise ValueError("nimail: 缺少邮箱地址")

    resp = tm_http.post(
        f"{BASE_URL}/api/getmails",
        headers={
            "User-Agent": _get_ua(),
            "Content-Type": "application/x-www-form-urlencoded",
            "Origin": BASE_URL,
            "Referer": f"{BASE_URL}/",
        },
        data=f"mail={quote(addr, safe='')}&time=0",
    )
    resp.raise_for_status()

    data = resp.json()
    if not isinstance(data, dict) or data.get("success") != "true":
        return []

    mail = data.get("mail")
    if not isinstance(mail, list):
        return []

    out: List[Email] = []
    for msg in mail:
        if not isinstance(msg, dict):
            continue
        row = {
            "id": _first(msg, "id", "time"),
            "from": _first(msg, "from", "sender"),
            "to": addr,
            "subject": _first(msg, "subject", "title"),
            "text": _first(msg, "text", "content"),
            "html": _first(msg, "html", "content"),
            "date": _first(msg, "date", "time"),
        }
        out.append(normalize_email(row, addr))
    return out
