"""
MailCatch 渠道 — https://mailcatch.com
公开收件箱，无需认证/Session。
创建邮箱: 随机用户名 + @mailcatch.com（无需 API 调用）。
获取邮件: GET /api/list/{inbox} 获取邮件列表（HTML），GET /api/data/{inbox}/{id} 获取邮件正文（HTML）。
域名: @mailcatch.com
"""

import random
import re
import string
from typing import List

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "mailcatch"
BASE_URL = "https://mailcatch.com"
DOMAIN = "mailcatch.com"

# 从邮件列表 HTML 中提取邮件项的正则
# 匹配: <div class="email-item" data-email-id="xxx" data-subject="xxx" data-timestamp="xxx" data-sender="xxx">
_EMAIL_ITEM_RE = re.compile(
    r'class="email-item"\s+'
    r'data-email-id="([^"]*)"\s+'
    r'data-subject="([^"]*)"\s+'
    r'data-timestamp="([^"]*)"\s+'
    r'data-sender="([^"]*)"',
    re.I | re.S,
)

_HEADERS = {
    "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
}


def _random_local() -> str:
    """生成随机用户名: sdk 前缀 + 16 位小写字母数字"""
    chars = string.ascii_lowercase + string.digits
    return "sdk" + "".join(random.choice(chars) for _ in range(16))


def _local_part(email: str) -> str:
    """提取邮箱地址的 @ 前部分"""
    return (email or "").strip().split("@")[0].strip()


def generate_email() -> EmailInfo:
    """
    创建 mailcatch 临时邮箱

    mailcatch.com 为公开收件箱，无需 API 调用即可创建。
    生成随机用户名并拼接 @mailcatch.com 域名。
    Token 存储用户名（@ 前部分），供后续获取邮件使用。
    """
    local = _random_local()
    email = f"{local}@{DOMAIN}"
    return EmailInfo(channel=CHANNEL, email=email, _token=local)


def get_emails(email: str, token: str) -> List[Email]:
    """
    获取 mailcatch 邮件列表

    流程:
    1. 从 token 获取收件箱名称（用户名）
    2. GET /api/list/{inbox} 获取邮件列表 HTML
    3. 正则提取每封邮件的 id/subject/timestamp/sender
    4. 对每封邮件 GET /api/data/{inbox}/{id} 获取 HTML 正文
    5. 使用 normalize_email 标准化返回结果
    """
    if not token:
        raise ValueError("mailcatch: token 不能为空")

    addr = (email or "").strip()
    if not addr:
        raise ValueError("mailcatch: 邮箱地址不能为空")

    # token 即为收件箱名称（用户名）
    inbox = token.strip()
    if not inbox:
        inbox = _local_part(addr)
    if not inbox:
        raise ValueError("mailcatch: 无法确定收件箱名称")

    # 获取邮件列表 HTML
    resp = tm_http.get(
        f"{BASE_URL}/api/list/{inbox}",
        headers=_HEADERS,
        timeout=15,
    )
    resp.raise_for_status()

    list_html = resp.text

    # 正则提取邮件项
    items = _EMAIL_ITEM_RE.findall(list_html)
    if not items:
        return []

    emails: List[Email] = []
    for email_id, subject, timestamp, sender in items:
        email_id = email_id.strip()
        if not email_id:
            continue

        # 获取邮件正文 HTML
        body_html = ""
        try:
            detail_resp = tm_http.get(
                f"{BASE_URL}/api/data/{inbox}/{email_id}",
                headers=_HEADERS,
                timeout=15,
            )
            if detail_resp.status_code == 200:
                body_html = detail_resp.text.strip()
        except Exception:
            pass

        raw_email = {
            "id": email_id,
            "from": sender.strip(),
            "to": addr,
            "subject": subject.strip(),
            "html": body_html,
            "date": timestamp.strip(),
        }
        emails.append(normalize_email(raw_email, addr))

    return emails
