"""
Nextmail 渠道 — https://nextmail.sh

创建: POST /temp-mail/inboxes（返回 id、address、token）
列表/详情需 Bearer 令牌；内部令牌格式为 `收件箱ID|读取令牌`。
详情正文位于 message.decodedBody（HTML）。
"""

from typing import List

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "nextmail"
BASE_URL = "https://api.nextmail.sh"


def _split_token(token: str) -> tuple:
    parts = (token or "").strip().split("|", 1)
    if len(parts) != 2 or not parts[0] or not parts[1]:
        raise ValueError("nextmail: 内部令牌格式错误")
    return parts[0], parts[1]


def _get_json(path: str, bearer: str) -> dict:
    resp = tm_http.get(
        f"{BASE_URL}{path}",
        headers={"Accept": "application/json", "Authorization": f"Bearer {bearer}"},
        timeout=20,
    )
    resp.raise_for_status()
    return resp.json()


def _from_email(value) -> str:
    if isinstance(value, dict):
        return str(value.get("email", "") or "")
    return str(value or "")


def generate_email(**kwargs) -> EmailInfo:
    """租用 Nextmail 临时收件箱"""
    resp = tm_http.post(
        f"{BASE_URL}/temp-mail/inboxes",
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
        raise ValueError("nextmail: 创建响应不完整")
    return EmailInfo(
        channel=CHANNEL,
        email=address,
        _token=f"{box_id}|{token}",
        expires_at=data.get("expiresAt"),
        created_at=data.get("createdAt"),
    )


def get_emails(email: str, token: str) -> List[Email]:
    """获取 Nextmail 邮件，详情正文位于 message.decodedBody"""
    box_id, bearer = _split_token(token)
    base = f"/temp-mail/inboxes/{box_id}/messages"
    data = _get_json(f"{base}?limit=50", bearer)
    out: List[Email] = []
    for item in data.get("data", []) or []:
        message_id = str(item.get("id", "")).strip()
        if not message_id:
            continue
        try:
            detail = _get_json(f"{base}/{message_id}", bearer)
            message = detail.get("message") or {}
            out.append(
                normalize_email(
                    {
                        "id": detail.get("id") or message_id,
                        "from": _from_email(detail.get("from")),
                        "to": detail.get("to") or email,
                        "subject": detail.get("subject"),
                        "html": message.get("decodedBody"),
                        "date": detail.get("receivedAt"),
                        "seen": detail.get("seen"),
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
                    "from": _from_email(item.get("from")),
                    "to": item.get("to") or email,
                    "subject": item.get("subject"),
                    "text": item.get("snippet"),
                    "date": item.get("receivedAt"),
                    "seen": item.get("seen"),
                },
                email,
            )
        )
    return out
