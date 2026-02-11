"""
temp-mail.io 渠道实现
API: https://api.internal.temp-mail.io/api/v3
需要动态获取 X-CORS-Header
"""

import re
import requests
from ..types import EmailInfo, Email
from ..normalize import normalize_email

CHANNEL = "temp-mail-io"
BASE_URL = "https://api.internal.temp-mail.io/api/v3"
PAGE_URL = "https://temp-mail.io/en"

_cached_cors_header = None


def _fetch_cors_header() -> str:
    """从 temp-mail.io 页面提取 mobileTestingHeader 值"""
    global _cached_cors_header
    if _cached_cors_header:
        return _cached_cors_header

    try:
        resp = requests.get(PAGE_URL, headers={
            "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36",
        }, timeout=10)
        match = re.search(r'mobileTestingHeader\s*:\s*"([^"]+)"', resp.text)
        if match:
            _cached_cors_header = match.group(1)
            return _cached_cors_header
    except Exception:
        pass

    _cached_cors_header = "1"
    return _cached_cors_header


def _get_api_headers() -> dict:
    """构建 API 请求头"""
    cors = _fetch_cors_header()
    return {
        "Content-Type": "application/json",
        "Application-Name": "web",
        "Application-Version": "4.0.0",
        "X-CORS-Header": cors,
        "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0",
        "origin": "https://temp-mail.io",
        "referer": "https://temp-mail.io/",
    }


def generate_email(**kwargs) -> EmailInfo:
    """创建临时邮箱"""
    headers = _get_api_headers()
    resp = requests.post(
        f"{BASE_URL}/email/new",
        headers=headers,
        json={"min_name_length": 10, "max_name_length": 10},
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()

    if not data.get("email") or not data.get("token"):
        raise Exception("Failed to generate email")

    return EmailInfo(
        channel=CHANNEL,
        email=data["email"],
        token=data["token"],
    )


def get_emails(email: str, **kwargs) -> list:
    """获取邮件列表"""
    headers = _get_api_headers()
    resp = requests.get(
        f"{BASE_URL}/email/{email}/messages",
        headers=headers,
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()

    raw_emails = data if isinstance(data, list) else []
    return [normalize_email(raw, email) for raw in raw_emails]
