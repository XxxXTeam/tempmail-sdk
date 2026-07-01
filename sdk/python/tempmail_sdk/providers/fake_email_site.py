"""
fake-email-site — https://fake-email.site
JSON REST API：创建临时邮箱 + 轮询收件箱
"""

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import EmailInfo, Email

CHANNEL = "fake-email-site"
BASE = "https://api.fake-email.site"

_HEADERS = {
    "Accept": "application/json",
    "Content-Type": "application/json",
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
}


def generate_email(**kwargs) -> EmailInfo:
    """创建临时邮箱，POST 空 JSON 对象，解析返回的 temp_email_addr。"""
    r = tm_http.post(f"{BASE}/api/temporary-address", headers=_HEADERS, json={})
    r.raise_for_status()
    data = r.json()
    if not isinstance(data, dict):
        raise ValueError(f"{CHANNEL}: 无效响应格式")
    email = (data.get("temp_email_addr") or "").strip()
    if not email:
        raise ValueError(f"{CHANNEL}: 响应中未找到临时邮箱地址")
    return EmailInfo(channel=CHANNEL, email=email, _token=email)


def get_emails(email: str, **kwargs) -> list[Email]:
    """轮询收件箱，GET /api/inbox/poll?address=xxx，解析 messages 数组并归一化。"""
    r = tm_http.get(f"{BASE}/api/inbox/poll?address={email}", headers=_HEADERS)
    if r.status_code == 404:
        return []
    r.raise_for_status()
    data = r.json() if r.content else {}
    if not isinstance(data, dict):
        return []
    rows = data.get("messages") or []
    if not isinstance(rows, list):
        return []
    out = []
    for row in rows:
        if not isinstance(row, dict):
            continue
        # 使用 normalize_email 统一归一化，normalize 会自动补 html 字段
        out.append(normalize_email(row, recipient_email=email))
    return out
