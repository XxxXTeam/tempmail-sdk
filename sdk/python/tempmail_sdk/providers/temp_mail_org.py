"""
temp-mail-org 渠道实现
网站: https://temp-mail.org
API Base: https://web2.temp-mail.org

流程：
  1. POST /mailbox 创建临时邮箱，返回 token 和邮箱地址
  2. GET /messages 获取邮件列表（需要 Bearer token）
  3. GET /messages/{id} 获取邮件详情（HTML 正文）
"""

from typing import List, Optional

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "temp-mail-org"
BASE_URL = "https://web2.temp-mail.org"

_HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
    "Origin": "https://temp-mail.org",
    "Referer": "https://temp-mail.org/",
}


def generate_email(duration: int = 30, domain: Optional[str] = None) -> EmailInfo:
    """
    创建 temp-mail.org 临时邮箱

    Args:
        duration: 有效期（分钟），本渠道不使用此参数
        domain: 域名筛选，本渠道不使用此参数

    Returns:
        EmailInfo 对象，token 为 JWT 字符串
    """
    resp = tm_http.post(
        f"{BASE_URL}/mailbox",
        headers=_HEADERS,
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()

    if not isinstance(data, dict):
        raise RuntimeError("temp-mail-org: 响应格式无效")

    token = data.get("token", "")
    mailbox = data.get("mailbox", "")

    if not token or not mailbox:
        raise RuntimeError(f"temp-mail-org: 创建邮箱失败，响应: {data!r}")

    return EmailInfo(channel=CHANNEL, email=mailbox, _token=token)


def _auth_headers(token: str) -> dict:
    """生成带 Bearer 认证的请求头"""
    headers = dict(_HEADERS)
    headers["Authorization"] = f"Bearer {token}"
    return headers


def get_emails(token: str, email: str) -> List[Email]:
    """
    获取 temp-mail.org 邮件列表

    流程:
    1. GET /messages 获取邮件列表（含基本信息）
    2. 逐封 GET /messages/{id} 获取邮件详情（含 HTML 正文）
    3. 使用 normalize_email 标准化输出

    Args:
        token: JWT 认证令牌
        email: 邮箱地址

    Returns:
        标准化邮件列表
    """
    if not token:
        raise ValueError("temp-mail-org: token 不能为空")

    addr = (email or "").strip()
    if not addr:
        raise ValueError("temp-mail-org: 邮箱地址不能为空")

    # 获取邮件列表
    resp = tm_http.get(
        f"{BASE_URL}/messages",
        headers=_auth_headers(token),
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()

    if not isinstance(data, dict):
        return []

    messages = data.get("messages")
    if not isinstance(messages, list) or not messages:
        return []

    out: List[Email] = []
    for msg in messages:
        if not isinstance(msg, dict):
            continue

        msg_id = msg.get("_id", "")
        if not msg_id:
            continue

        # 获取邮件详情以取得完整正文
        detail = _fetch_message_detail(token, msg_id)

        raw = {
            "id": msg_id,
            "from": detail.get("from", msg.get("from", "")),
            "to": addr,
            "subject": detail.get("subject", msg.get("subject", "")),
            "text": detail.get("bodyPreview", msg.get("bodyPreview", "")),
            "html": detail.get("bodyHtml", ""),
            "date": detail.get("createdAt", str(msg.get("receivedAt", ""))),
        }
        out.append(normalize_email(raw, addr))

    return out


def _fetch_message_detail(token: str, msg_id: str) -> dict:
    """
    获取单封邮件详情

    Args:
        token: JWT 认证令牌
        msg_id: 邮件 ID

    Returns:
        邮件详情字典，失败时返回空字典
    """
    try:
        resp = tm_http.get(
            f"{BASE_URL}/messages/{msg_id}",
            headers=_auth_headers(token),
            timeout=15,
        )
        resp.raise_for_status()
        data = resp.json()
        return data if isinstance(data, dict) else {}
    except Exception:
        return {}
