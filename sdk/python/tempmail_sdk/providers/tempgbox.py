"""
TempGBox 渠道实现
API: https://tempgbox.net/api/proxy
"""

import base64
import secrets
from .. import http as tm_http
from ..normalize import normalize_email
from ..types import EmailInfo

CHANNEL = "tempgbox"
API_URL = "https://tempgbox.net/api/proxy"
ORIGIN = "https://tempgbox.net"

DEFAULT_HEADERS = {
    "Accept": "text/html,application/json",
    "Content-Type": "application/json",
    "Origin": ORIGIN,
    "Referer": f"{ORIGIN}/",
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
}


def _random_device_id() -> str:
    """上游按 X-Device-ID 限制连续生成；每个新邮箱使用新的设备 ID。"""
    return secrets.token_hex(32)


def _random_ip() -> str:
    return ".".join(str(secrets.randbelow(254) + 1) for _ in range(4))


def _decode_payload(html: str) -> dict:
    marker = 'data-x="'
    start = html.find(marker)
    quote = '"'
    if start < 0:
        marker = "data-x='"
        start = html.find(marker)
        quote = "'"
    if start < 0:
        raise Exception("tempgbox: missing encoded response payload")
    start += len(marker)
    end = html.find(quote, start)
    if end < 0:
        raise Exception("tempgbox: malformed encoded response payload")
    raw = base64.b64decode(html[start:end]).decode("utf-8")
    import json

    return json.loads(raw)


def _post_proxy(route: str, device_id: str, payload: dict) -> dict:
    ip = _random_ip()
    resp = tm_http.post(
        f"{API_URL}?route={route}",
        headers={
            **DEFAULT_HEADERS,
            "X-Device-ID": device_id,
            "X-Forwarded-For": ip,
            "X-Real-IP": ip,
            "X-Originating-IP": ip,
        },
        json=payload,
    )
    data = _decode_payload(resp.text)
    if not resp.ok:
        reason = (
            data.get("detail")
            or data.get("error")
            or data.get("message")
            or resp.reason
        )
        raise Exception(f"tempgbox {route} failed: {resp.status_code} {reason}")
    return data


def generate_email(**kwargs) -> EmailInfo:
    """创建临时邮箱"""
    device_id = _random_device_id()
    data = _post_proxy("generate", device_id, {"variant": "googlemail"})
    alias = data.get("alias") or {}
    email = alias.get("email") or alias.get("alias") or ""
    if not email:
        raise Exception("tempgbox: missing email")

    return EmailInfo(
        channel=CHANNEL,
        email=email,
        _token=device_id,
        created_at=alias.get("created_at"),
        expires_at=alias.get("expires_at"),
    )


def get_emails(token: str, email: str = "", **kwargs) -> list:
    """获取邮件列表"""
    if not token:
        raise Exception("tempgbox: missing device id")
    data = _post_proxy("inbox", token, {"email": email})
    messages = data.get("messages") or []
    out = []
    for raw in messages:
        if not isinstance(raw, dict):
            continue
        flat = {
            **raw,
            "from": raw.get("from") or raw.get("sender", ""),
            "text": raw.get("text") or raw.get("body_text", ""),
            "html": raw.get("html") or raw.get("body_html", ""),
            "date": raw.get("date") or raw.get("received_at", ""),
            "messageId": raw.get("messageId")
            or raw.get("message_id")
            or raw.get("id", ""),
            "attachments": raw.get("attachments") or raw.get("attachments_info") or [],
        }
        out.append(normalize_email(flat, email))
    return out
