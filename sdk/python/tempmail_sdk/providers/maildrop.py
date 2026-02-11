"""
maildrop.cc 渠道实现
API: GraphQL https://api.maildrop.cc/graphql
"""

import random
import string
import email
import email.header
import email.policy
from .. import http as tm_http
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


def _extract_text_from_mime(raw: str) -> str:
    """
    从原始 MIME 邮件源码中提取纯文本正文
    maildrop 的 data 字段返回完整 MIME 源码，需要解析出 text/plain 部分
    """
    if not raw:
        return ""
    try:
        msg = email.message_from_string(raw, policy=email.policy.default)
        # 优先提取 text/plain 部分
        body = msg.get_body(preferencelist=("plain",))
        if body:
            content = body.get_content()
            return content.strip() if isinstance(content, str) else content.decode("utf-8", errors="replace").strip()
        # 回退到 text/html 并去除标签
        body = msg.get_body(preferencelist=("html",))
        if body:
            content = body.get_content()
            return content.strip() if isinstance(content, str) else content.decode("utf-8", errors="replace").strip()
    except Exception:
        pass
    # 解析失败则尝试跳过邮件头返回正文
    idx = raw.find("\r\n\r\n")
    return raw[idx + 4:].strip() if idx >= 0 else raw


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
        token=username,
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
                emails.append(Email(
                    id=msg.get("id") or item.get("id", ""),
                    from_addr=_decode_rfc2047(msg.get("headerfrom") or item.get("headerfrom", "")),
                    to=email,
                    subject=_decode_rfc2047(msg.get("subject") or item.get("subject", "")),
                    text=_extract_text_from_mime(msg.get("data", "")),
                    html=msg.get("html", ""),
                    date=msg.get("date") or item.get("date", ""),
                    is_read=False,
                    attachments=[],
                ))
        except Exception:
            pass

    return emails
