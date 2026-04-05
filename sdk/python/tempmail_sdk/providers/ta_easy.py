"""
ta-easy.com 临时邮箱
API: https://api-endpoint.ta-easy.com/temp-email/
"""

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import EmailInfo

CHANNEL = "ta-easy"
API_BASE = "https://api-endpoint.ta-easy.com"
ORIGIN = "https://www.ta-easy.com"

_HEADERS = {
    "Accept": "application/json",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
    "origin": ORIGIN,
    "referer": f"{ORIGIN}/",
}


def generate_email(**kwargs) -> EmailInfo:
    """POST /temp-email/address/new（空 body）"""
    resp = tm_http.post(
        f"{API_BASE}/temp-email/address/new",
        headers={**_HEADERS, "Content-Length": "0"},
        data=b"",
    )
    resp.raise_for_status()
    data = resp.json()
    if data.get("status") != "success" or not data.get("address") or not data.get("token"):
        msg = data.get("message") or "create failed"
        raise RuntimeError(f"ta-easy: {msg}")
    exp = data.get("expiresAt")
    expires_at = int(exp) if isinstance(exp, (int, float)) else None
    return EmailInfo(
        channel=CHANNEL,
        email=data["address"],
        _token=data["token"],
        expires_at=expires_at,
    )


def get_emails(email: str, token: str, **kwargs) -> list:
    """POST /temp-email/inbox/list"""
    resp = tm_http.post(
        f"{API_BASE}/temp-email/inbox/list",
        headers={**_HEADERS, "Content-Type": "application/json"},
        json={"token": token, "email": email},
    )
    resp.raise_for_status()
    data = resp.json()
    if data.get("status") != "success":
        msg = data.get("message") or "inbox failed"
        raise RuntimeError(f"ta-easy: {msg}")
    raw_list = data.get("data")
    if not isinstance(raw_list, list):
        raw_list = []
    return [normalize_email(raw, email) for raw in raw_list]
