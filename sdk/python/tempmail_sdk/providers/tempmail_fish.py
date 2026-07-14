"""
TempMail Fish 渠道 — https://tempmail.fish

流程:
  1. 创建: POST /emails/new-email（无 body）→ {"email":"...","authKey":"...","emails":[]}
     token 存储为 JSON 字符串 {"email":"...","authKey":"..."}
  2. 读信: GET /emails/emails?emailAddress={email}
     请求头 Authorization: {authKey}（直接放 authKey，不带 Bearer 前缀）
     响应为邮件数组，每元素字段 to/from/subject/textBody/htmlBody/attachments/timestamp
"""

import json
from datetime import datetime, timezone
from typing import Any, Dict, List
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "tempmail-fish"
API_BASE = "https://api.tempmail.fish"
HEADERS = {
    "Accept": "application/json",
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
}


def _to_iso(timestamp: Any) -> str:
    """将毫秒时间戳转换为 ISO8601 字符串，非法值返回空串"""
    if timestamp is None:
        return ""
    try:
        millis = float(timestamp)
    except (TypeError, ValueError):
        return ""
    return datetime.fromtimestamp(millis / 1000.0, tz=timezone.utc).isoformat()


def _flatten_message(raw: Dict[str, Any], recipient: str) -> Dict[str, Any]:
    """将 tempmail.fish 原始邮件映射为标准化中间格式"""
    body = str(raw.get("textBody") or "")
    return {
        "from": str(raw.get("from") or ""),
        "to": str(raw.get("to") or "") or recipient,
        "subject": str(raw.get("subject") or ""),
        # textBody 中常内嵌 HTML，交由 normalize_email 自动识别归位
        "text": body,
        "html": str(raw.get("htmlBody") or ""),
        "date": _to_iso(raw.get("timestamp")),
        "is_read": False,
        "attachments": raw.get("attachments") or [],
    }


def generate_email() -> EmailInfo:
    """
    创建 tempmail.fish 临时邮箱
    token 存储为 JSON: {"email":"...","authKey":"..."}
    """
    resp = tm_http.post(
        f"{API_BASE}/emails/new-email",
        headers={**HEADERS, "Content-Type": "application/json"},
    )
    resp.raise_for_status()

    data = resp.json()
    if not isinstance(data, dict):
        raise ValueError("tempmail-fish: 创建邮箱响应无效")

    email = str(data.get("email") or "").strip()
    auth_key = str(data.get("authKey") or "").strip()
    if not email or "@" not in email or not auth_key:
        raise ValueError("tempmail-fish: 创建邮箱响应无效")

    token = json.dumps({"email": email, "authKey": auth_key})
    return EmailInfo(channel=CHANNEL, email=email, _token=token)


def get_emails(token: str, email: str = "") -> List[Email]:
    """
    获取 tempmail.fish 邮件列表
    token 为 JSON 字符串，包含 email 与 authKey
    """
    if not token:
        raise ValueError("tempmail-fish: token 不能为空")

    try:
        parsed = json.loads(token)
    except (ValueError, TypeError):
        raise ValueError("tempmail-fish: token 格式无效")
    if not isinstance(parsed, dict):
        raise ValueError("tempmail-fish: token 格式无效")

    address = str(parsed.get("email") or email or "").strip()
    auth_key = str(parsed.get("authKey") or "").strip()
    if not address or not auth_key:
        raise ValueError("tempmail-fish: token 缺少 email 或 authKey")

    resp = tm_http.get(
        f"{API_BASE}/emails/emails?emailAddress={quote(address, safe='')}",
        headers={**HEADERS, "Authorization": auth_key},
    )
    resp.raise_for_status()

    data = resp.json()
    if isinstance(data, list):
        rows = data
    elif isinstance(data, dict) and isinstance(data.get("emails"), list):
        rows = data["emails"]
    else:
        rows = []

    emails: List[Email] = []
    for row in rows:
        if isinstance(row, dict):
            emails.append(normalize_email(_flatten_message(row, address), address))
    return emails
