"""
temp-mail.io 渠道实现
API: https://api.internal.temp-mail.io/api/v3
"""

import re
from typing import List, Optional

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "temp-mail-io"
BASE_URL = "https://api.internal.temp-mail.io/api/v3"
PAGE_URL = "https://temp-mail.io/en"

_cached_cors_header: Optional[str] = None


def _fetch_cors_header() -> str:
    global _cached_cors_header
    if _cached_cors_header:
        return _cached_cors_header

    try:
        resp = tm_http.get(
            PAGE_URL,
            headers={
                "User-Agent": (
                    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                    "AppleWebKit/537.36 (KHTML, like Gecko) "
                    "Chrome/144.0.0.0 Safari/537.36"
                )
            },
            timeout=15,
        )
        html = resp.text
        match = re.search(r'mobileTestingHeader\s*:\s*"([^"]+)"', html)
        if match:
            _cached_cors_header = match.group(1)
            return _cached_cors_header
    except Exception:
        pass

    _cached_cors_header = "1"
    return _cached_cors_header


def _headers() -> dict:
    return {
        "Content-Type": "application/json",
        "Application-Name": "web",
        "Application-Version": "4.0.0",
        "X-CORS-Header": _fetch_cors_header(),
        "User-Agent": (
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
            "AppleWebKit/537.36 (KHTML, like Gecko) "
            "Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0"
        ),
        "Origin": "https://temp-mail.io",
        "Referer": "https://temp-mail.io/",
    }


def generate_email() -> EmailInfo:
    """创建 temp-mail.io 临时邮箱"""
    resp = tm_http.post(
        f"{BASE_URL}/email/new",
        headers=_headers(),
        json={"min_name_length": 10, "max_name_length": 10},
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()

    email = data.get("email") if isinstance(data, dict) else ""
    token = data.get("token") if isinstance(data, dict) else ""
    if not email or not token:
        raise ValueError("temp-mail-io: invalid generate response")

    return EmailInfo(channel=CHANNEL, email=email, _token=token)


def get_emails(email: str) -> List[Email]:
    """获取 temp-mail.io 邮件列表"""
    resp = tm_http.get(
        f"{BASE_URL}/email/{email}/messages",
        headers=_headers(),
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    rows = data if isinstance(data, list) else []
    return [normalize_email(raw, email) for raw in rows if isinstance(raw, dict)]
