"""
10minutemail-net 渠道 — https://10minutemail.net

10 分钟有效期的临时邮箱，通过 PHP session 管理邮箱生命周期。

流程:
  1. 创建: GET https://10minutemail.net/address.api.php
     响应: {"mail_get_mail": "xxx@laoia.com", "mail_list": [...], ...}
     Token 序列化为 JSON: {"cookie": "PHPSESSID=xxx"}
  2. 读信: GET https://10minutemail.net/address.api.php (带 session cookie)
     获取 mail_list 后逐封请求详情:
     GET https://10minutemail.net/mail.api.php?mailid={mail_id}
"""

import json
from typing import List, Optional

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "10minutemail-net"
BASE_URL = "https://10minutemail.net"

_HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
    "Accept": "application/json",
}


def generate_email(duration: int = 10, domain: Optional[str] = None) -> EmailInfo:
    """
    创建 10minutemail.net 临时邮箱

    Args:
        duration: 有效期（分钟），本渠道固定 10 分钟
        domain: 域名筛选，本渠道不使用此参数

    Returns:
        EmailInfo 对象，token 为 JSON 字符串 {"cookie":"PHPSESSID=xxx"}
    """
    resp = tm_http.get(f"{BASE_URL}/address.api.php", headers=_HEADERS, timeout=15)
    resp.raise_for_status()
    data = resp.json()

    if not isinstance(data, dict):
        raise RuntimeError("10minutemail-net: 响应格式无效")

    address = data.get("mail_get_mail", "")
    if not address:
        raise RuntimeError(f"10minutemail-net: 创建邮箱失败，响应: {data!r}")

    # 从响应 cookie 中提取 PHPSESSID
    phpsessid = resp.cookies.get("PHPSESSID", "")
    if not phpsessid:
        raise RuntimeError("10minutemail-net: 未获取到 PHPSESSID cookie")

    token = json.dumps({"cookie": f"PHPSESSID={phpsessid}"})

    # 计算过期时间
    expires_at = None
    duetime = data.get("mail_get_duetime")
    if duetime:
        try:
            expires_at = int(duetime) * 1000
        except (ValueError, TypeError):
            pass

    return EmailInfo(
        channel=CHANNEL,
        email=address,
        _token=token,
        expires_at=expires_at,
    )


def get_emails(token: str, email: str) -> List[Email]:
    """
    获取 10minutemail.net 邮件列表

    Args:
        token: generate_email 返回的 token (JSON 字符串)
        email: 邮箱地址

    Returns:
        标准化的 Email 列表
    """
    token_data = json.loads(token)
    cookie_str = token_data.get("cookie", "")

    headers = {**_HEADERS, "Cookie": cookie_str}

    # 获取邮件列表
    resp = tm_http.get(f"{BASE_URL}/address.api.php", headers=headers, timeout=15)
    resp.raise_for_status()
    data = resp.json()

    mail_list = data.get("mail_list", [])
    if not isinstance(mail_list, list):
        return []

    out: List[Email] = []
    for item in mail_list:
        if not isinstance(item, dict):
            continue

        mail_id = item.get("mail_id", "")
        # 排除系统欢迎邮件
        if not mail_id or mail_id == "welcome":
            continue

        # 获取邮件详情
        detail_resp = tm_http.get(
            f"{BASE_URL}/mail.api.php?mailid={mail_id}",
            headers=headers,
            timeout=15,
        )
        detail_resp.raise_for_status()
        detail = detail_resp.json()

        if not isinstance(detail, dict):
            continue

        # 提取正文：优先 HTML，其次纯文本
        html_body = ""
        text_body = ""
        body_list = detail.get("body", [])
        if isinstance(body_list, list):
            for part in body_list:
                if not isinstance(part, dict):
                    continue
                content_type = part.get("content", "")
                if "text/html" in content_type:
                    html_body = part.get("body", "")
                elif "text/plain" in content_type:
                    text_body = part.get("body", "")

        # plain 字段作为备选纯文本
        if not text_body:
            plain_list = detail.get("plain", [])
            if isinstance(plain_list, list) and plain_list:
                text_body = plain_list[0] if isinstance(plain_list[0], str) else ""

        raw = {
            "id": mail_id,
            "from": detail.get("from", "") or item.get("from", ""),
            "to": detail.get("to", "") or email,
            "subject": detail.get("subject", "") or item.get("subject", ""),
            "text": text_body,
            "html": html_body,
            "date": detail.get("datetime", "") or item.get("datetime", ""),
        }
        out.append(normalize_email(raw, email))

    return out
