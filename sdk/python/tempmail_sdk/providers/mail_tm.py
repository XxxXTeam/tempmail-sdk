"""
mail.tm 渠道实现
API: https://api.mail.tm
流程: 获取域名 → 生成随机邮箱/密码 → 创建账号 → 获取 Bearer Token
"""

import random
import string
import requests
from ..types import EmailInfo, Email
from ..normalize import normalize_email

CHANNEL = "mail-tm"
BASE_URL = "https://api.mail.tm"

DEFAULT_HEADERS = {
    "Content-Type": "application/json",
    "Accept": "application/json",
}


def _random_string(length: int) -> str:
    return "".join(random.choices(string.ascii_lowercase + string.digits, k=length))


def _get_domains() -> list:
    """获取可用域名列表，兼容 Hydra 格式和纯数组"""
    resp = requests.get(f"{BASE_URL}/domains", headers=DEFAULT_HEADERS, timeout=15)
    resp.raise_for_status()
    data = resp.json()
    members = data if isinstance(data, list) else (data.get("hydra:member") or [])
    return [d["domain"] for d in members if d.get("isActive")]


def _create_account(address: str, password: str) -> dict:
    """创建账号"""
    resp = requests.post(
        f"{BASE_URL}/accounts",
        headers={**DEFAULT_HEADERS, "Content-Type": "application/ld+json"},
        json={"address": address, "password": password},
        timeout=15,
    )
    resp.raise_for_status()
    return resp.json()


def _get_token(address: str, password: str) -> str:
    """获取 Bearer Token"""
    resp = requests.post(
        f"{BASE_URL}/token",
        headers=DEFAULT_HEADERS,
        json={"address": address, "password": password},
        timeout=15,
    )
    resp.raise_for_status()
    return resp.json()["token"]


def _flatten_message(msg: dict, recipient_email: str) -> dict:
    """将 mail.tm 消息格式扁平化"""
    from_addr = ""
    if isinstance(msg.get("from"), dict):
        from_addr = msg["from"].get("address", "")
    elif isinstance(msg.get("from"), str):
        from_addr = msg["from"]

    to_addr = recipient_email
    if isinstance(msg.get("to"), list) and msg["to"]:
        if isinstance(msg["to"][0], dict):
            to_addr = msg["to"][0].get("address", recipient_email)

    html_content = msg.get("html", "")
    if isinstance(html_content, list):
        html_content = "".join(html_content)

    attachments = []
    for a in (msg.get("attachments") or []):
        att = {
            "filename": a.get("filename", ""),
            "size": a.get("size"),
            "contentType": a.get("contentType"),
        }
        if a.get("downloadUrl"):
            att["downloadUrl"] = f"{BASE_URL}{a['downloadUrl']}"
        attachments.append(att)

    return {
        "id": msg.get("id", ""),
        "from": from_addr,
        "to": to_addr,
        "subject": msg.get("subject", ""),
        "text": msg.get("text", ""),
        "html": html_content,
        "createdAt": msg.get("createdAt", ""),
        "seen": msg.get("seen", False),
        "attachments": attachments,
    }


def generate_email(**kwargs) -> EmailInfo:
    """创建临时邮箱"""
    domains = _get_domains()
    if not domains:
        raise Exception("No available domains")

    domain = random.choice(domains)
    username = _random_string(12)
    address = f"{username}@{domain}"
    password = _random_string(16)

    account = _create_account(address, password)
    token = _get_token(address, password)

    return EmailInfo(
        channel=CHANNEL,
        email=address,
        token=token,
        created_at=account.get("createdAt"),
    )


def get_emails(token: str, email: str = "", **kwargs) -> list:
    """获取邮件列表"""
    # 1. 获取邮件列表
    resp = requests.get(
        f"{BASE_URL}/messages",
        headers={**DEFAULT_HEADERS, "Authorization": f"Bearer {token}"},
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()

    # 兼容 Hydra 格式和纯数组
    messages = data if isinstance(data, list) else (data.get("hydra:member") or [])
    if not messages:
        return []

    # 2. 逐封获取详情
    result = []
    for msg in messages:
        try:
            detail_resp = requests.get(
                f"{BASE_URL}/messages/{msg['id']}",
                headers={**DEFAULT_HEADERS, "Authorization": f"Bearer {token}"},
                timeout=15,
            )
            if detail_resp.ok:
                detail = detail_resp.json()
                flat = _flatten_message(detail, email)
            else:
                flat = _flatten_message(msg, email)
        except Exception:
            flat = _flatten_message(msg, email)

        result.append(normalize_email(flat, email))

    return result
