"""
temp.tf 渠道 — https://temp.tf

创建: GET /api/account?plus=1 → {email}
收件: POST /api/check {email} → {data:[...]}
body 按 bodyContentType 归位为 text 或 html。
"""

from typing import List

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "temp-tf"
BASE_URL = "https://temp.tf/api"
_HEADERS = {"Accept": "application/json", "Content-Type": "application/json"}


def generate_email(**kwargs) -> EmailInfo:
    """获取一个一次性地址"""
    resp = tm_http.get(f"{BASE_URL}/account?plus=1", headers=_HEADERS, timeout=20)
    resp.raise_for_status()
    address = str(resp.json().get("email", "")).strip()
    if not address:
        raise ValueError("temp-tf: 创建响应缺少地址")
    return EmailInfo(channel=CHANNEL, email=address)


def get_emails(email: str) -> List[Email]:
    """获取 temp.tf 邮件，body 按 bodyContentType 归位"""
    address = (email or "").strip()
    if not address:
        raise ValueError("temp-tf: 邮箱地址为空")

    resp = tm_http.post(
        f"{BASE_URL}/check",
        headers=_HEADERS,
        json={"email": address},
        timeout=20,
    )
    resp.raise_for_status()
    out: List[Email] = []
    for item in resp.json().get("data", []) or []:
        is_text = str(item.get("bodyContentType", "")).lower() == "text"
        body = item.get("body") or ""
        out.append(
            normalize_email(
                {
                    "id": item.get("id"),
                    "from": item.get("from"),
                    "to": address,
                    "subject": item.get("subject"),
                    "text": body if is_text else "",
                    "html": "" if is_text else body,
                    "date": item.get("date"),
                    "attachments": item.get("attachments"),
                },
                address,
            )
        )
    return out
