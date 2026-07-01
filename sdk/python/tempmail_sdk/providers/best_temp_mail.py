"""
BestTempMail 渠道 — https://best-temp-mail.com
纯 JSON REST API，无需认证，无需 Session。
创建邮箱: POST /api/v3/createEmail，客户端生成 UUID 作为 intToken。
获取邮件: POST /api/v3/getEmailList，需要 address + id + intToken + update_tag。
"""

import json
import uuid
from typing import List

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "best-temp-mail"
BASE = "https://best-temp-mail.com"

# 公共请求头
_HEADERS = {
    "Content-Type": "application/json",
    "Referer": f"{BASE}/",
    "Origin": BASE,
}


def generate_email(channel: str = CHANNEL) -> EmailInfo:
    """
    创建 best-temp-mail 临时邮箱

    流程:
    1. 生成客户端 UUID 作为 intToken
    2. POST /api/v3/createEmail 创建邮箱
    3. 将 intToken、id、update_tag 序列化为 JSON 存入 token
    """
    int_token = str(uuid.uuid4())

    resp = tm_http.post(
        f"{BASE}/api/v3/createEmail",
        headers=_HEADERS,
        json={"intToken": int_token},
    )
    resp.raise_for_status()

    body = resp.json()
    if body.get("status") != "success":
        raise RuntimeError(f"best-temp-mail: 创建邮箱失败: {body}")

    data = body.get("data", {})
    address = data.get("address", "")
    email_id = data.get("id", "")
    update_tag = data.get("update_tag", "")

    if not address or "@" not in address:
        raise RuntimeError(f"best-temp-mail: 返回的邮箱地址无效: {address!r}")

    # 将 intToken、id、update_tag 序列化为 JSON 存入 token
    token = json.dumps({
        "intToken": int_token,
        "id": email_id,
        "update_tag": update_tag,
    }, separators=(",", ":"))

    return EmailInfo(channel=channel, email=address, _token=token)


def get_emails(token: str, email: str = "") -> List[Email]:
    """
    获取 best-temp-mail 邮件列表

    流程:
    1. 从 token 中反序列化出 intToken、id、update_tag
    2. POST /api/v3/getEmailList 获取邮件
    3. 使用 normalize_email 标准化返回结果
    """
    if not token:
        raise ValueError("best-temp-mail: token 不能为空")

    addr = (email or "").strip()
    if not addr:
        raise ValueError("best-temp-mail: 邮箱地址不能为空")

    # 解析 token
    try:
        tok_data = json.loads(token)
    except (json.JSONDecodeError, ValueError) as e:
        raise ValueError("best-temp-mail: 无效的 token") from e

    int_token = tok_data.get("intToken", "")
    email_id = tok_data.get("id", "")
    update_tag = tok_data.get("update_tag", "")

    if not int_token or not email_id:
        raise ValueError("best-temp-mail: token 中缺少必要字段 (intToken / id)")

    resp = tm_http.post(
        f"{BASE}/api/v3/getEmailList",
        headers=_HEADERS,
        json={
            "address": addr,
            "id": email_id,
            "intToken": int_token,
            "update_tag": update_tag,
        },
    )
    resp.raise_for_status()

    body = resp.json()
    data = body.get("data", {})

    # 无新邮件时 hasNewEmail 为 false
    if not data.get("hasNewEmail", False):
        return []

    raw_emails = data.get("emails", [])
    if not isinstance(raw_emails, list):
        return []

    out: List[Email] = []
    for item in raw_emails:
        if not isinstance(item, dict):
            continue
        raw = {
            "id": str(item.get("id", "")),
            "from": item.get("from", item.get("from_address", item.get("sender", ""))),
            "to": addr,
            "subject": item.get("subject", ""),
            "text": item.get("text", item.get("body_text", "")),
            "html": item.get("html", item.get("body_html", item.get("body", ""))),
            "date": item.get("date", item.get("created_at", "")),
        }
        out.append(normalize_email(raw, addr))

    return out
