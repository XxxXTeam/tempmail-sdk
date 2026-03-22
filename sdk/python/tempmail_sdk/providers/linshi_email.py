"""
linshi-email.com 渠道实现
API: https://www.linshi-email.com/api/v1
"""

import time
from urllib.parse import quote
from .. import http as tm_http
from ..types import EmailInfo
from ..normalize import normalize_email
from .linshi_token import random_synthetic_linshi_api_path_key

CHANNEL = "linshi-email"
BASE_URL = "https://www.linshi-email.com/api/v1"

DEFAULT_HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36",
    "Content-Type": "application/json",
    "Origin": "https://www.linshi-email.com",
    "Referer": "https://www.linshi-email.com/",
    "sec-ch-ua": '"Microsoft Edge";v="143", "Chromium";v="143", "Not A(Brand";v="24"',
    "sec-ch-ua-mobile": "?0",
    "sec-ch-ua-platform": '"Windows"',
    "DNT": "1",
}


def _parse_email_from_data(data) -> str:
    if not isinstance(data, dict):
        return ""
    raw = (
        (isinstance(data.get("email"), str) and data.get("email"))
        or (isinstance(data.get("mail"), str) and data.get("mail"))
        or (isinstance(data.get("address"), str) and data.get("address"))
        or ""
    )
    s = str(raw).strip()
    return s if s and "@" in s else ""


def generate_email(**kwargs) -> EmailInfo:
    """创建临时邮箱"""
    api_path_key = random_synthetic_linshi_api_path_key()
    resp = tm_http.post(
        f"{BASE_URL}/email/{api_path_key}",
        headers=DEFAULT_HEADERS,
        json={},
    )
    resp.raise_for_status()
    data = resp.json()

    if data.get("status") != "ok":
        raise Exception("Failed to generate email")

    d = data.get("data") or {}
    email = _parse_email_from_data(d)
    if not email:
        raise Exception(
            "linshi-email: API 未返回有效邮箱（可能触发频率限制：每小时约 10 个 / 每天约 20 个）"
        )

    return EmailInfo(
        channel=CHANNEL,
        email=email,
        _token=api_path_key,
        expires_at=d.get("expired"),
    )


def get_emails(email: str, api_path_key: str, **kwargs) -> list:
    """获取邮件列表（须与创建时相同的 api_path_key）"""
    if not api_path_key:
        raise ValueError("api_path_key is required for linshi-email channel")
    ts = int(time.time() * 1000)
    resp = tm_http.get(
        f"{BASE_URL}/refreshmessage/{api_path_key}/{quote(email)}?t={ts}",
        headers=DEFAULT_HEADERS,
    )
    resp.raise_for_status()
    data = resp.json()

    if data.get("status") != "ok":
        raise Exception("Failed to get emails")

    return [normalize_email(raw, email) for raw in (data.get("list") or [])]
