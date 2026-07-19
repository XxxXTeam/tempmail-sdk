"""
dropmail-me 渠道 — https://dropmail.me

GraphQL 临时邮箱服务，需自行生成认证 token。

流程:
  1. 生成 token: 从页面提取 data-k，反转 + base64 解码得到 secret，
     用 FNV hash 构造 token
  2. 创建 session: GraphQL mutation introduceSession
  3. 读信: GraphQL query session(id) { mails { ... } }
"""

import base64
import json
import random
import re
import string
from datetime import datetime
from typing import List, Optional

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "dropmail-me"
BASE_URL = "https://dropmail.me"

_HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
    "Accept": "application/json",
    "Content-Type": "application/json",
}


def _fnv_hash(s: str) -> str:
    """FNV-1a 变体哈希"""
    h = 2166136261
    for c in s:
        h ^= ord(c)
        h += (h << 1) + (h << 4) + (h << 7) + (h << 8) + (h << 24)
        h &= 0xFFFFFFFF
    return format(h, 'x')


def _generate_auth_token() -> str:
    """生成 dropmail.me API 认证 token"""
    # 获取页面，提取 data-k
    resp = tm_http.get(f"{BASE_URL}/en/", headers={
        "User-Agent": _HEADERS["User-Agent"],
        "Accept": "text/html",
    }, timeout=15)
    resp.raise_for_status()

    match = re.search(r'<meta\s+name="app-config"\s+data-k="([^"]+)"', resp.text)
    if not match:
        raise RuntimeError("dropmail-me: 无法从页面提取 data-k")

    data_k = match.group(1)
    # 反转 + base64 解码得到 secret
    _secret = base64.b64decode(data_k[::-1]).decode("utf-8")

    # 生成随机部分
    date_str = datetime.utcnow().strftime("%Y%m%d")
    random_part = date_str + ''.join(
        random.choices(string.ascii_letters + string.digits, k=16)
    )

    # 计算哈希
    hash_input = random_part + _secret
    h = _fnv_hash(hash_input)

    return f"website_{random_part}_{h}"


def generate_email(duration: int = 30, domain: Optional[str] = None) -> EmailInfo:
    """
    创建 dropmail.me 临时邮箱

    Args:
        duration: 有效期（分钟），本渠道不使用此参数
        domain: 域名筛选，本渠道不使用此参数

    Returns:
        EmailInfo 对象
    """
    auth_token = _generate_auth_token()

    # 创建 session
    query = "mutation { introduceSession { id expiresAt addresses { address } } }"
    resp = tm_http.post(
        f"{BASE_URL}/api/graphql/{auth_token}",
        headers=_HEADERS,
        json={"query": query},
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()

    session_data = data.get("data", {}).get("introduceSession")
    if not session_data:
        raise RuntimeError(f"dropmail-me: 创建 session 失败，响应: {data!r}")

    session_id = session_data.get("id", "")
    addresses = session_data.get("addresses", [])

    if not session_id or not addresses:
        raise RuntimeError(f"dropmail-me: 响应中缺少 session ID 或地址，响应: {data!r}")

    address = addresses[0].get("address", "")
    if not address:
        raise RuntimeError(f"dropmail-me: 地址为空，响应: {data!r}")

    # Token 序列化为 JSON
    composite_token = json.dumps({
        "session_id": session_id,
        "auth_token": auth_token,
    })

    # 解析过期时间
    expires_at = None
    expires_str = session_data.get("expiresAt")
    if expires_str:
        try:
            dt = datetime.fromisoformat(expires_str.replace("Z", "+00:00"))
            expires_at = int(dt.timestamp() * 1000)
        except (ValueError, TypeError):
            pass

    return EmailInfo(
        channel=CHANNEL,
        email=address,
        _token=composite_token,
        expires_at=expires_at,
    )


def get_emails(token: str, email: str) -> List[Email]:
    """
    获取 dropmail.me 邮件列表

    Args:
        token: JSON 字符串 {"session_id":"...","auth_token":"..."}
        email: 邮箱地址

    Returns:
        标准化邮件列表
    """
    if not token:
        raise ValueError("dropmail-me: token 不能为空")

    addr = (email or "").strip()
    if not addr:
        raise ValueError("dropmail-me: 邮箱地址不能为空")

    # 解析复合 token
    try:
        token_data = json.loads(token)
    except (json.JSONDecodeError, TypeError):
        raise ValueError("dropmail-me: token 格式无效，应为 JSON 字符串")

    session_id = token_data.get("session_id", "")
    auth_token = token_data.get("auth_token", "")

    if not session_id or not auth_token:
        raise ValueError("dropmail-me: token 中缺少 session_id 或 auth_token 字段")

    # 查询邮件
    query = (
        '{ session(id:"' + session_id + '") '
        '{ mails { id headerFrom headerSubject text html receivedAt } } }'
    )
    resp = tm_http.post(
        f"{BASE_URL}/api/graphql/{auth_token}",
        headers=_HEADERS,
        json={"query": query},
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()

    session_resp = data.get("data", {}).get("session")
    if not session_resp:
        return []

    mails = session_resp.get("mails")
    if not isinstance(mails, list) or not mails:
        return []

    out: List[Email] = []
    for msg in mails:
        if not isinstance(msg, dict):
            continue

        raw = {
            "id": msg.get("id", ""),
            "from": msg.get("headerFrom", ""),
            "to": addr,
            "subject": msg.get("headerSubject", ""),
            "text": msg.get("text", ""),
            "html": msg.get("html", ""),
            "date": msg.get("receivedAt", ""),
        }
        out.append(normalize_email(raw, addr))

    return out
