"""
Neighbours 渠道 — https://neighbours.sh

公共收件箱模式，任意用户名即可收信，无需注册。
  1. 创建: 本地生成随机用户名 sdk + 10 位小写字母数字 @ neighbours.sh，token 存邮箱地址本身
  2. 读信:
     a. GET /inbox/{address} 取邮件摘要列表（含 uid）
     b. 对每个 uid GET /inbox/{address}/{uid} 补全完整正文
"""

import random
import string
from typing import Any, Dict, List, Optional
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "neighbours-sh"
API_BASE = "https://neighbours.sh/api/v1"
DOMAIN = "neighbours.sh"
HEADERS = {
    "Accept": "application/json",
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
}


def _random_local() -> str:
    """生成随机用户名，前缀 sdk + 10 位小写字母数字"""
    chars = string.ascii_lowercase + string.digits
    return "sdk" + "".join(random.choice(chars) for _ in range(10))


def _first_address(value: Any) -> str:
    """从 from/to 结构中提取首个可用地址"""
    if value is None:
        return ""
    if isinstance(value, str):
        return value.strip()
    if isinstance(value, list):
        for item in value:
            hit = _first_address(item)
            if hit:
                return hit
        return ""
    if isinstance(value, dict):
        address = str(value.get("address") or "").strip()
        if address:
            return address
        text = str(value.get("text") or "").strip()
        if "@" in text:
            return text
        return _first_address(value.get("value"))
    return str(value).strip()


def _request_json(path: str, allow_404: bool = False) -> Optional[Dict[str, Any]]:
    """请求 JSON，allow_404 时 404 返回 None"""
    resp = tm_http.get(f"{API_BASE}{path}", headers=HEADERS, timeout=15)
    if allow_404 and resp.status_code == 404:
        return None
    resp.raise_for_status()
    data = resp.json()
    return data if isinstance(data, dict) else None


def _flatten_message(detail: Dict[str, Any], recipient: str) -> Dict[str, Any]:
    """将 neighbours.sh 邮件详情映射为标准化中间格式"""
    return {
        "id": str(detail.get("uid") or ""),
        "from": _first_address(detail.get("from")),
        "to": _first_address(detail.get("to")) or recipient,
        "subject": str(detail.get("subject") or ""),
        "text": str(detail.get("text") or ""),
        "html": str(detail.get("html") or ""),
        "date": str(detail.get("date") or ""),
        "is_read": False,
        "attachments": detail.get("attachments") or [],
    }


def _fetch_detail(address: str, uid: str) -> Optional[Dict[str, Any]]:
    """获取单封邮件详情"""
    data = _request_json(
        f"/inbox/{quote(address, safe='')}/{quote(uid, safe='')}", allow_404=True
    )
    if isinstance(data, dict):
        detail = data.get("data")
        if isinstance(detail, dict):
            return detail
    return None


def generate_email() -> EmailInfo:
    """
    创建 neighbours.sh 临时邮箱
    公共收件箱，无需 API 调用，本地生成随机地址，token 存邮箱地址本身
    """
    email = f"{_random_local()}@{DOMAIN}"
    return EmailInfo(channel=CHANNEL, email=email, _token=email)


def get_emails(email: str) -> List[Email]:
    """
    获取 neighbours.sh 邮件列表
    先请求列表拿 uid 数组，再逐个请求详情提取完整正文
    """
    address = (email or "").strip()
    if not address:
        raise ValueError("neighbours-sh: 缺少邮箱地址")

    data = _request_json(f"/inbox/{quote(address, safe='')}", allow_404=True)
    if not data:
        return []
    rows = data.get("data")
    if not isinstance(rows, list):
        return []

    emails: List[Email] = []
    for item in rows:
        if not isinstance(item, dict):
            continue
        uid = str(item.get("uid") or "").strip()
        if not uid:
            continue
        detail = _fetch_detail(address, uid)
        if detail is None:
            continue
        emails.append(normalize_email(_flatten_message(detail, address), address))
    return emails
