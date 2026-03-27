"""
mail.gw 渠道实现
API 与 mail.tm 完全一致，仅 baseURL 不同
"""

import random
import string
from ..types import EmailInfo
from ..normalize import normalize_email
from .. import http as tm_http

CHANNEL = "mail-gw"
BASE_URL = "https://api.mail.gw"

DEFAULT_HEADERS = {
    "Content-Type": "application/json",
    "Accept": "application/json",
}


def _random_string(length):
    return "".join(random.choices(string.ascii_lowercase + string.digits, k=length))


def _get_domains():
    resp = tm_http.get(f"{BASE_URL}/domains", headers=DEFAULT_HEADERS)
    resp.raise_for_status()
    data = resp.json()
    members = data if isinstance(data, list) else data.get("hydra:member", [])
    return [d["domain"] for d in members if d.get("isActive")]


def _create_account(address, password):
    resp = tm_http.post(
        f"{BASE_URL}/accounts",
        headers={**DEFAULT_HEADERS, "Content-Type": "application/ld+json"},
        json={"address": address, "password": password},
    )
    resp.raise_for_status()
    return resp.json()


def _get_token(address, password):
    resp = tm_http.post(
        f"{BASE_URL}/token",
        headers=DEFAULT_HEADERS,
        json={"address": address, "password": password},
    )
    resp.raise_for_status()
    return resp.json()["token"]


def generate_email():
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
        _token=token,
        created_at=account.get("createdAt"),
    )


def _flatten_message(msg, recipient_email):
    return {
        "id": msg.get("id", ""),
        "from": (msg.get("from") or {}).get("address", ""),
        "to": (msg.get("to") or [{}])[0].get("address", "") or recipient_email,
        "subject": msg.get("subject", ""),
        "text": msg.get("text", ""),
        "html": "".join(msg["html"]) if isinstance(msg.get("html"), list) else msg.get("html", ""),
        "createdAt": msg.get("createdAt", ""),
        "seen": msg.get("seen", False),
        "attachments": [
            {
                "filename": a.get("filename", ""),
                "size": a.get("size"),
                "contentType": a.get("contentType"),
                "downloadUrl": f"{BASE_URL}{a['downloadUrl']}" if a.get("downloadUrl") else None,
            }
            for a in msg.get("attachments", [])
        ],
    }


def get_emails(token, email="", **kwargs):
    """获取邮件列表"""
    resp = tm_http.get(
        f"{BASE_URL}/messages",
        headers={**DEFAULT_HEADERS, "Authorization": f"Bearer {token}"},
    )
    resp.raise_for_status()
    data = resp.json()
    messages = data if isinstance(data, list) else data.get("hydra:member", [])

    if not messages:
        return []

    emails = []
    for msg in messages:
        try:
            detail_resp = tm_http.get(
                f"{BASE_URL}/messages/{msg['id']}",
                headers={**DEFAULT_HEADERS, "Authorization": f"Bearer {token}"},
            )
            detail_resp.raise_for_status()
            detail = detail_resp.json()
            flat = _flatten_message(detail, email)
        except Exception:
            flat = _flatten_message(msg, email)
        emails.append(normalize_email(flat, email))

    return emails
