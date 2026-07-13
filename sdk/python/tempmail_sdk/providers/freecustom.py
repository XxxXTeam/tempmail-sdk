"""
freecustom 渠道 — https://www.freecustom.email

免注册临时邮箱，任意 local part @ 可用域名即时可收信，Token 即邮箱地址本身。
读信时动态获取匿名 JWT（有效期约 2 小时），无需持久化。

流程:
  1. 创建: GET https://api2.freecustom.email/domains 取域名，筛选 tier=="free" 且
     expiring_soon 非 true 的域名随机选一个，本地拼接随机 local part。
  2. 读信:
     a. POST /api/auth 取匿名 JWT
     b. GET /api/public-mailbox?fullMailboxId=<email> 取邮件元数据列表
     c. 对每封 GET /api/public-mailbox?fullMailboxId=<email>&messageId=<id> 补全正文
"""

import random
import string
from typing import Any, Dict, List
from urllib.parse import quote

from ..config import get_config
from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "freecustom"
SITE_URL = "https://www.freecustom.email"
DOMAINS_URL = "https://api2.freecustom.email/domains"
REFERER = "https://www.freecustom.email/en"


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
    挑选一个当前可收信的域名。

    api2 /domains 无需鉴权，返回域名及过期标记，优先选择 tier=="free" 且
    未过期（expiring_soon 非 true）的域名；若全部标记过期则退回全量列表随机。
    """
    resp = tm_http.get(
        DOMAINS_URL,
        headers={
            "User-Agent": _get_ua(),
            "Accept": "application/json",
            "Referer": REFERER,
        },
    )
    resp.raise_for_status()

    data = resp.json()
    lst = data.get("data") if isinstance(data, dict) else None
    if not isinstance(lst, list) or not lst:
        raise RuntimeError("freecustom: 域名列表为空")

    usable = [
        d
        for d in lst
        if isinstance(d, dict)
        and d.get("tier") == "free"
        and d.get("expiring_soon") is not True
        and d.get("domain")
    ]
    pool = (
        usable
        if usable
        else [d for d in lst if isinstance(d, dict) and d.get("domain")]
    )
    if not pool:
        raise RuntimeError("freecustom: 无可用域名")

    picked = random.choice(pool)
    return str(picked["domain"])


def _fetch_auth_token() -> str:
    """
    获取匿名访问令牌（JWT，有效期约 2 小时）
    POST /api/auth → {"token": "<JWT>"}
    """
    resp = tm_http.post(
        f"{SITE_URL}/api/auth",
        headers={
            "User-Agent": _get_ua(),
            "Accept": "application/json",
            "Content-Type": "application/json",
            "Referer": REFERER,
        },
    )
    resp.raise_for_status()

    data = resp.json()
    if not isinstance(data, dict) or not data.get("token"):
        raise RuntimeError("freecustom: 令牌响应无效")
    return str(data["token"])


def generate_email() -> EmailInfo:
    """创建 freecustom.email 临时邮箱"""
    domain = _pick_domain()
    email = f"{_random_local(10)}@{domain}"
    return EmailInfo(channel=CHANNEL, email=email, _token=email)


def get_emails(email: str) -> List[Email]:
    """
    获取 freecustom.email 邮件列表

    1. POST /api/auth 取 JWT
    2. GET /api/public-mailbox?fullMailboxId=<email> 取邮件元数据列表
    3. 对每封 GET /api/public-mailbox?fullMailboxId=<email>&messageId=<id> 补全正文
       正文补全失败时退回列表元数据
    """
    addr = (email or "").strip()
    if not addr:
        raise ValueError("freecustom: 缺少邮箱地址")

    jwt = _fetch_auth_token()
    auth_headers = {
        "User-Agent": _get_ua(),
        "Accept": "application/json",
        "Referer": REFERER,
        "Authorization": f"Bearer {jwt}",
        "x-fce-client": "web-client",
    }

    list_resp = tm_http.get(
        f"{SITE_URL}/api/public-mailbox?fullMailboxId={quote(addr, safe='')}",
        headers=auth_headers,
    )
    list_resp.raise_for_status()

    list_data = list_resp.json()
    if not isinstance(list_data, dict) or not list_data.get("success"):
        return []

    items = list_data.get("data")
    if not isinstance(items, list):
        return []

    out: List[Email] = []
    for item in items:
        if not isinstance(item, dict) or not item.get("id"):
            continue
        msg_id = str(item["id"])

        full: Dict[str, Any] = item
        try:
            msg_resp = tm_http.get(
                f"{SITE_URL}/api/public-mailbox?fullMailboxId={quote(addr, safe='')}"
                f"&messageId={quote(msg_id, safe='')}",
                headers=auth_headers,
            )
            if msg_resp.ok:
                msg_data = msg_resp.json()
                if (
                    isinstance(msg_data, dict)
                    and msg_data.get("success")
                    and isinstance(msg_data.get("data"), dict)
                ):
                    full = msg_data["data"]
        except (ValueError, OSError):
            # 正文补全失败时退回列表元数据
            pass

        row = {
            "id": _first(full, "id"),
            "from": _first(full, "from"),
            "to": _first(full, "to") or addr,
            "subject": _first(full, "subject"),
            "text": _first(full, "text"),
            "html": _first(full, "html"),
            "date": _first(full, "date"),
        }
        out.append(normalize_email(row, addr))
    return out
