"""
MailTemp.cc 渠道 — https://mailtemp.cc
PHP POST form-urlencoded API，无需认证。
创建收件箱: POST /api.php body: action=inbox，返回 JSON 字符串格式的用户名。
获取邮件: POST /api.php body: action=fetch&inbox={username}&last_id=0。
查看邮件详情: POST /api.php body: action=view&id={id}&inbox={username}。
域名: @neocea.com
"""

import json
from typing import List

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "mailtemp-cc"
BASE = "https://mailtemp.cc"
DOMAIN = "neocea.com"

# 公共请求头
_HEADERS = {
    "Content-Type": "application/x-www-form-urlencoded",
    "Accept": "*/*",
    "Referer": f"{BASE}/",
    "Origin": BASE,
}


def generate_email(channel: str = CHANNEL) -> EmailInfo:
    """
    创建 mailtemp.cc 临时邮箱

    流程:
    1. POST /api.php body: action=inbox 获取用户名
    2. 返回值为 JSON 字符串格式（带引号），需用 json.loads() 解析
    3. 拼接完整邮箱地址: {username}@neocea.com
    4. token 存储用户名（@ 前面的部分）
    """
    resp = tm_http.post(
        f"{BASE}/api.php",
        headers=_HEADERS,
        data="action=inbox",
    )
    resp.raise_for_status()

    # 返回值为 JSON 字符串格式（如 "vindictiverate"），需去掉引号
    raw_text = resp.text.strip()
    try:
        username = json.loads(raw_text)
    except (json.JSONDecodeError, ValueError):
        # 兜底：如果不是 JSON 字符串格式，直接使用原始文本
        username = raw_text

    if not username or not isinstance(username, str):
        raise RuntimeError(f"mailtemp-cc: 返回的用户名无效: {raw_text!r}")

    address = f"{username}@{DOMAIN}"
    return EmailInfo(channel=channel, email=address, _token=username)


def get_emails(email: str, token: str) -> List[Email]:
    """
    获取 mailtemp.cc 邮件列表

    流程:
    1. POST /api.php body: action=fetch&inbox={token}&last_id=0 获取邮件列表
    2. 遍历列表，对每封邮件 POST action=view&id={id}&inbox={token} 获取详情
    3. 使用 normalize_email 标准化返回结果
    """
    if not token:
        raise ValueError("mailtemp-cc: token 不能为空")

    addr = (email or "").strip()
    if not addr:
        raise ValueError("mailtemp-cc: 邮箱地址不能为空")

    # 获取邮件列表
    resp = tm_http.post(
        f"{BASE}/api.php",
        headers=_HEADERS,
        data=f"action=fetch&inbox={token}&last_id=0",
    )
    resp.raise_for_status()

    raw_list = resp.json()
    if not isinstance(raw_list, list):
        return []

    out: List[Email] = []
    for item in raw_list:
        if not isinstance(item, dict):
            continue

        mail_id = item.get("id", "")
        if not mail_id:
            continue

        # 获取邮件详情（包含 body_html）
        detail_resp = tm_http.post(
            f"{BASE}/api.php",
            headers=_HEADERS,
            data=f"action=view&id={mail_id}&inbox={token}",
        )
        detail_resp.raise_for_status()
        detail = detail_resp.json()
        if not isinstance(detail, dict):
            detail = {}

        raw = {
            "id": str(mail_id),
            "from": item.get("sender_email", item.get("sender", "")),
            "to": addr,
            "subject": item.get("subject", detail.get("subject", "")),
            "text": "",
            "html": detail.get("body_html", ""),
            "date": item.get("received_at", detail.get("received_at", "")),
        }
        out.append(normalize_email(raw, addr))

    return out
