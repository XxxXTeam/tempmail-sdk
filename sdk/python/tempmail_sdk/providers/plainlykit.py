"""
PlainlyKit 渠道 — https://plainlykit.com

创建: POST /tools/temporary-email/api/mailboxes（返回 id、address、token）
列表/详情需 Bearer 令牌；内部令牌格式为 `邮箱ID|读取令牌`。
"""

from typing import List

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "plainlykit"
BASE_URL = "https://plainlykit.com/tools/temporary-email/api"


def _split_token(token: str) -> tuple:
    parts = (token or "").strip().split("|", 1)
    if len(parts) != 2 or not parts[0] or not parts[1]:
        raise ValueError("plainlykit: 内部令牌格式错误")
    return parts[0], parts[1]


def _get_json(path: str, bearer: str) -> dict:
    resp = tm_http.get(
        f"{BASE_URL}{path}",
        headers={"Accept": "application/json", "Authorization": f"Bearer {bearer}"},
        timeout=20,
    )
    resp.raise_for_status()
    return resp.json()


def generate_email(**kwargs) -> EmailInfo:
    """创建 PlainlyKit 临时邮箱"""
    resp = tm_http.post(
        f"{BASE_URL}/mailboxes",
        headers={"Accept": "application/json", "Content-Type": "application/json"},
        json={},
        timeout=20,
    )
    resp.raise_for_status()
    data = resp.json()
    address = str(data.get("address", "")).strip()
    box_id = str(data.get("id", "")).strip()
    token = str(data.get("token", "")).strip()
    if not address or not box_id or not token:
        raise ValueError("plainlykit: 创建响应不完整")
    return EmailInfo(
        channel=CHANNEL,
        email=address,
        _token=f"{box_id}|{token}",
        expires_at=data.get("expiresAt"),
        created_at=data.get("createdAt"),
    )


def get_emails(email: str, token: str) -> List[Email]:
    """获取 PlainlyKit 邮件，详情提供纯文本正文"""
    box_id, bearer = _split_token(token)
    base = f"/mailboxes/{box_id}/messages"
    data = _get_json(base, bearer)
    out: List[Email] = []
    for item in data.get("messages", []) or []:
        message_id = str(item.get("id", "")).strip()
        if not message_id:
            continue
        try:
            detail = _get_json(f"{base}/{message_id}", bearer)
            raw = detail.get("message") or detail
            out.append(
                normalize_email(
                    {
                        "id": raw.get("id") or message_id,
                        "from": raw.get("sender") or item.get("sender"),
                        "to": email,
                        "subject": raw.get("subject") or item.get("subject"),
                        "text": raw.get("text"),
                        "html": raw.get("html"),
                        "date": raw.get("receivedAt"),
                    },
                    email,
                )
            )
            continue
        except Exception:
            pass
        out.append(
            normalize_email(
                {
                    "id": message_id,
                    "from": item.get("sender"),
                    "to": email,
                    "subject": item.get("subject"),
                    "text": item.get("preview"),
                    "date": item.get("receivedAt"),
                },
                email,
            )
        )
    return out
