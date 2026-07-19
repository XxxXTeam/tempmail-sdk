"""
restmail-net 渠道 — https://restmail.net

Mozilla 开源项目，无需创建邮箱，ad-hoc 模式。
随机生成 username，邮箱即为 username@restmail.net。
收件: GET https://restmail.net/mail/{username}
"""

import random
import string
from typing import List

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "restmail-net"
BASE_URL = "https://restmail.net"

_HEADERS = {
    "Accept": "application/json",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36"
    ),
}


def _random_username() -> str:
    """生成 10-12 位随机用户名"""
    length = random.randint(10, 12)
    return "".join(random.choices(string.ascii_lowercase + string.digits, k=length))


def generate_email() -> EmailInfo:
    """
    创建 restmail.net 临时邮箱
    无需请求服务端，随机生成 username 即可使用
    """
    username = _random_username()
    email = f"{username}@restmail.net"
    return EmailInfo(channel=CHANNEL, email=email)


def get_emails(email: str) -> List[Email]:
    """
    获取 restmail.net 邮件列表
    GET /mail/{username}，返回 JSON 数组
    """
    address = (email or "").strip()
    if not address:
        raise ValueError("restmail-net: 邮箱地址为空")

    # 从邮箱地址中提取 username（@前面部分）
    username = address.split("@")[0]
    if not username:
        raise ValueError("restmail-net: 无法提取用户名")

    resp = tm_http.get(
        f"{BASE_URL}/mail/{username}",
        headers=_HEADERS,
        timeout=15,
    )
    if resp.status_code == 404:
        return []
    resp.raise_for_status()

    data = resp.json()
    if not isinstance(data, list):
        return []

    out: List[Email] = []
    for msg in data:
        if not isinstance(msg, dict):
            continue

        # 提取发件人：优先 from[0].address，其次 headers.from
        from_addr = ""
        from_list = msg.get("from")
        if isinstance(from_list, list) and from_list:
            first = from_list[0]
            if isinstance(first, dict):
                from_addr = first.get("address", "")
        if not from_addr:
            headers = msg.get("headers")
            if isinstance(headers, dict):
                from_addr = headers.get("from", "")

        # 邮件正文：优先 html，其次 text
        html_body = msg.get("html", "") or ""
        text_body = msg.get("text", "") or ""

        raw = {
            "id": msg.get("id", ""),
            "from": from_addr,
            "to": address,
            "subject": msg.get("subject", ""),
            "text": text_body,
            "html": html_body,
            "date": msg.get("receivedAt", ""),
            "is_read": False,
            "attachments": [],
        }
        out.append(normalize_email(raw, address))

    return out
