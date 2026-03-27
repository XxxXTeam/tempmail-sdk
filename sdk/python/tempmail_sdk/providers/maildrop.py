"""
maildrop.cc 渠道实现
API: GraphQL https://api.maildrop.cc/graphql
"""

import random
import re
import string
import email
import email.header
import email.policy
from .. import http as tm_http
from ..logger import get_logger
from ..types import EmailInfo, Email

CHANNEL = "maildrop"
GRAPHQL_URL = "https://api.maildrop.cc/graphql"
DOMAIN = "maildrop.cc"


def _decode_rfc2047(s: str) -> str:
    """
    解码 RFC 2047 编码的邮件头
    如 =?utf-8?B?b3BlbmVs?= 解码为可读文本
    """
    if not s:
        return ""
    decoded_parts = email.header.decode_header(s)
    result = []
    for part, charset in decoded_parts:
        if isinstance(part, bytes):
            result.append(part.decode(charset or "utf-8", errors="replace"))
        else:
            result.append(part)
    return " ".join(result)


def _strip_html_to_text(html: str) -> str:
    if not html:
        return ""
    return re.sub(r"\s+", " ", re.sub(r"<[^>]*>", " ", html)).strip()


def _part_content_str(body) -> str:
    content = body.get_content()
    if isinstance(content, str):
        return content.strip()
    return content.decode("utf-8", errors="replace").strip()


def _extract_plain_from_mime(raw: str) -> str:
    """仅提取 text/plain（与 npm 行为一致：无 plain 时不从 html 填 text）。"""
    if not raw:
        return ""
    try:
        msg = email.message_from_string(raw, policy=email.policy.default)
        body = msg.get_body(preferencelist=("plain",))
        if body:
            return _part_content_str(body)
    except Exception:
        idx = raw.find("\r\n\r\n")
        if idx >= 0:
            return raw[idx + 4 :].strip()
        idx = raw.find("\n\n")
        if idx >= 0:
            return raw[idx + 2 :].strip()
        return raw.strip()
    return ""


def _extract_html_from_mime(raw: str) -> str:
    if not raw:
        return ""
    try:
        msg = email.message_from_string(raw, policy=email.policy.default)
        body = msg.get_body(preferencelist=("html",))
        if body:
            return _part_content_str(body)
    except Exception:
        get_logger().debug("maildrop: extract html from mime failed", exc_info=True)
    return ""


def _random_username(length: int = 10) -> str:
    return "".join(random.choices(string.ascii_lowercase + string.digits, k=length))


def _graphql_request(
    operation_name: str, query: str, variables: dict = None
) -> dict:
    """
    发送 GraphQL 请求
    使用 operationName + variables 的标准 GraphQL 格式
    """
    resp = tm_http.post(
        GRAPHQL_URL,
        headers={
            "Content-Type": "application/json",
            "Origin": "https://maildrop.cc",
            "Referer": "https://maildrop.cc/",
            "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
        },
        json={
            "operationName": operation_name,
            "variables": variables or {},
            "query": query,
        },
    )
    resp.raise_for_status()
    result = resp.json()

    if result.get("errors"):
        raise Exception(f"Maildrop GraphQL error: {result['errors'][0].get('message')}")

    return result.get("data", {})


def generate_email(**kwargs) -> EmailInfo:
    """创建临时邮箱，任意用户名即可接收邮件"""
    username = _random_username()
    email = f"{username}@{DOMAIN}"

    # 验证 API 可用
    _graphql_request(
        "GetInbox",
        "query GetInbox($mailbox: String!) { inbox(mailbox: $mailbox) { id } }",
        {"mailbox": username},
    )

    return EmailInfo(
        channel=CHANNEL,
        email=email,
        _token=username,
    )


def get_emails(token: str, email: str = "", **kwargs) -> list:
    """获取邮件列表"""
    mailbox = token or email.split("@")[0]

    # 查询收件箱
    data = _graphql_request(
        "GetInbox",
        "query GetInbox($mailbox: String!) { inbox(mailbox: $mailbox) { id headerfrom subject date } }",
        {"mailbox": mailbox},
    )
    inbox = data.get("inbox") or []
    if not inbox:
        return []

    # 逐封获取完整内容
    emails = []
    for item in inbox:
        try:
            msg_data = _graphql_request(
                "GetMessage",
                "query GetMessage($mailbox: String!, $id: String!) { message(mailbox: $mailbox, id: $id) { id headerfrom subject date data html } }",
                {"mailbox": mailbox, "id": item["id"]},
            )
            msg = msg_data.get("message")
            if msg:
                raw_data = msg.get("data", "") or ""
                plain = _extract_plain_from_mime(raw_data)
                from_mime = _extract_html_from_mime(raw_data)
                api_html = (msg.get("html") or "").strip()
                html_out = api_html if api_html else from_mime
                text_out = plain if plain else _strip_html_to_text(html_out)
                emails.append(Email(
                    id=msg.get("id") or item.get("id", ""),
                    from_addr=_decode_rfc2047(msg.get("headerfrom") or item.get("headerfrom", "")),
                    to=email,
                    subject=_decode_rfc2047(msg.get("subject") or item.get("subject", "")),
                    text=text_out,
                    html=html_out,
                    date=msg.get("date") or item.get("date", ""),
                    is_read=False,
                    attachments=[],
                ))
        except Exception:
            get_logger().debug("maildrop: get_emails single message failed", exc_info=True)

    return emails
