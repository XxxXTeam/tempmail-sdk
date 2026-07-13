"""
MyTempMail.cc 渠道 — https://mytempmail.cc
简单的 REST API，通过 JSON 交互。
创建邮箱: POST /api/address body: {"domain":"nilvaro.com","name":"<random>","expiry":600}
获取邮件: GET /api/mails/<token>
"""

import random
import string
import time
from typing import List

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "mytempmail-cc"
BASE_URL = "https://api.mytempmail.cc"
DEFAULT_DOMAIN = "nilvaro.com"
DEFAULT_EXPIRY = 600

# 公共请求头
_HEADERS = {
    "Content-Type": "application/json",
    "Accept": "application/json",
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
}


def _random_name(length: int = 10) -> str:
    """生成随机用户名，仅包含小写字母和数字"""
    return "".join(random.choices(string.ascii_lowercase + string.digits, k=length))


def generate_email(channel: str = CHANNEL) -> EmailInfo:
    """
    创建 mytempmail.cc 临时邮箱

    流程:
    1. 生成随机用户名
    2. POST /api/address 发送 JSON 请求体
    3. 响应包含 token、address、expires_in
    4. token 用于后续获取邮件
    """
    name = _random_name()
    resp = tm_http.post(
        f"{BASE_URL}/api/address",
        headers=_HEADERS,
        json={
            "domain": DEFAULT_DOMAIN,
            "name": name,
            "expiry": DEFAULT_EXPIRY,
        },
    )
    resp.raise_for_status()

    data = resp.json()
    if not isinstance(data, dict):
        raise RuntimeError("mytempmail-cc: 响应格式无效")

    token = data.get("token", "")
    address = data.get("address", "")
    expires_in = data.get("expires_in", DEFAULT_EXPIRY)

    if not token or not address or "@" not in address:
        raise RuntimeError(f"mytempmail-cc: 创建邮箱失败，响应: {data!r}")

    # 计算过期时间戳（毫秒）
    expires_at = int((time.time() + expires_in) * 1000)

    return EmailInfo(
        channel=channel,
        email=address,
        _token=token,
        expires_at=expires_at,
    )


def get_emails(token: str, email: str) -> List[Email]:
    """
    获取 mytempmail.cc 邮件列表

    流程:
    1. GET /api/mails/<token> 获取邮件列表
    2. 响应格式: {"results":[...],"count":0,"limit":10}
    3. 遍历 results 数组，使用 normalize_email 标准化
    """
    if not token:
        raise ValueError("mytempmail-cc: token 不能为空")

    addr = (email or "").strip()
    if not addr:
        raise ValueError("mytempmail-cc: 邮箱地址不能为空")

    resp = tm_http.get(
        f"{BASE_URL}/api/mails/{token}",
        headers=_HEADERS,
    )
    resp.raise_for_status()

    data = resp.json()
    if not isinstance(data, dict):
        return []

    results = data.get("results")
    if not isinstance(results, list):
        return []

    out: List[Email] = []
    for item in results:
        if not isinstance(item, dict):
            continue
        raw = {
            "id": item.get("id", ""),
            "from": item.get("from", item.get("from_address", item.get("sender", ""))),
            "to": item.get("to", addr),
            "subject": item.get("subject", ""),
            "text": item.get("text", item.get("body_text", "")),
            "html": item.get("html", item.get("body_html", "")),
            "date": item.get("date", item.get("received_at", "")),
        }
        out.append(normalize_email(raw, addr))

    return out
