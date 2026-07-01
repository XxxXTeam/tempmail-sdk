"""
maildrop.cc 临时邮箱渠道

GraphQL API，无认证。
创建邮箱: 无需 API，直接生成随机用户名 + "@maildrop.cc"（公共邮箱，任何人可查看任意地址收件箱）
获取邮件: POST https://api.maildrop.cc/graphql
  - inbox(mailbox) 查询邮件列表（id/headerfrom/subject/date，无正文）
  - message(mailbox,id) 查询单封详情（含 html 正文）
本渠道为公共邮箱，无需持久化认证信息；token 仅为占位以满足上层校验。
"""

import json
import secrets
from typing import List

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "maildrop-cc"
DOMAIN = "maildrop.cc"
GRAPHQL_URL = "https://api.maildrop.cc/graphql"

_HEADERS = {
    "Accept": "application/json",
    "Content-Type": "application/json",
    "Origin": "https://maildrop.cc",
    "Referer": "https://maildrop.cc/",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
}

_LOCAL_CHARS = "abcdefghijklmnopqrstuvwxyz0123456789"


def _random_local(n: int = 10) -> str:
    """生成随机用户名（小写字母 + 数字）"""
    return "".join(secrets.choice(_LOCAL_CHARS) for _ in range(n))


def _mailbox(email: str) -> str:
    """提取地址 @ 前的用户名部分"""
    email = (email or "").strip()
    idx = email.find("@")
    return email[:idx] if idx >= 0 else email


def _do_graphql(query: str) -> dict:
    """发送 GraphQL 请求并返回解析后的 JSON"""
    resp = tm_http.post(GRAPHQL_URL, headers=_HEADERS, json={"query": query})
    resp.raise_for_status()
    return resp.json()


def _inbox_query(mailbox: str) -> str:
    """构造 inbox 列表查询语句（mailbox 经 JSON 序列化，避免转义/注入问题）"""
    mb = json.dumps(mailbox)
    return f"query {{ inbox(mailbox: {mb}) {{ id headerfrom subject date }} }}"


def _message_query(mailbox: str, mid: str) -> str:
    """构造 message 单封详情查询语句"""
    mb = json.dumps(mailbox)
    mid_s = json.dumps(mid)
    return (
        f"query {{ message(mailbox: {mb}, id: {mid_s}) "
        f"{{ id headerfrom subject date html }} }}"
    )


def generate_email(channel: str = CHANNEL) -> EmailInfo:
    """
    创建 maildrop.cc 临时邮箱

    无需 API 调用，直接生成随机用户名 + "@maildrop.cc"。
    token 不参与后续逻辑，使用邮箱地址占位以满足上层非空校验。
    """
    email = f"{_random_local(10)}@{DOMAIN}"
    return EmailInfo(channel=channel, email=email, _token=email)


def get_emails(email: str, token: str = "") -> List[Email]:
    """
    获取 maildrop.cc 邮件列表

    先用 inbox 查询拿到 id 列表，再逐封用 message 查询补全 html 正文。
    maildrop.cc 为公共邮箱服务，token 不参与逻辑。
    """
    mailbox = _mailbox(email)
    if not mailbox:
        raise ValueError("maildrop-cc: 邮箱地址为空")

    # 1. 查询邮件列表
    inbox_resp = _do_graphql(_inbox_query(mailbox))
    items = ((inbox_resp or {}).get("data") or {}).get("inbox")
    if not isinstance(items, list) or not items:
        return []

    out: List[Email] = []
    for item in items:
        if not isinstance(item, dict):
            continue
        mid = str(item.get("id") or "")

        # 2. 查询单封详情补全 html 正文，失败时回退使用列表元信息
        raw = {
            "id": mid,
            "from": item.get("headerfrom", ""),
            "subject": item.get("subject", ""),
            "date": item.get("date", ""),
        }
        if mid:
            try:
                msg_resp = _do_graphql(_message_query(mailbox, mid))
                msg = ((msg_resp or {}).get("data") or {}).get("message")
                if isinstance(msg, dict):
                    raw = {
                        "id": msg.get("id", mid),
                        "from": msg.get("headerfrom", ""),
                        "subject": msg.get("subject", ""),
                        "date": msg.get("date", ""),
                        "html": msg.get("html", ""),
                    }
            except (ValueError, OSError):
                pass

        out.append(normalize_email(raw, email))

    return out
