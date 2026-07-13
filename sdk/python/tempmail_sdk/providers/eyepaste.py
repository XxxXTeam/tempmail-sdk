"""
Eyepaste 渠道 — https://eyepaste.com
RSS XML API，无认证，直接生成随机用户名
"""

import random
import re
import string
import xml.etree.ElementTree as ET
from typing import List

from .. import http as tm_http
from ..types import Email, EmailInfo

CHANNEL = "eyepaste"
DOMAIN = "eyepaste.com"
BASE = "https://www.eyepaste.com"

HEADERS = {
    "Accept": "application/rss+xml, application/xml, text/xml, */*",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
}


def _random_username(length: int = 10) -> str:
    """生成随机用户名"""
    return "".join(random.choices(string.ascii_lowercase + string.digits, k=length))


def generate_email(channel: str = CHANNEL) -> EmailInfo:
    """生成随机邮箱地址，无需 API 调用"""
    username = _random_username()
    email = f"{username}@{DOMAIN}"
    return EmailInfo(channel=channel, email=email)


def _parse_description(desc: str) -> dict:
    """
    解析 RSS description 中的邮件信息
    第一个 <p> 包含 " From: {from} To: {to} Subject: {subject} Date: {date}"
    后续内容是邮件正文
    """
    result = {
        "from": "",
        "to": "",
        "subject": "",
        "date": "",
        "body": "",
    }

    if not desc:
        return result

    # 提取第一个 <p>...</p> 中的元数据
    p_match = re.search(r"<p[^>]*>(.*?)</p>", desc, re.DOTALL | re.IGNORECASE)
    if p_match:
        meta = p_match.group(1).strip()
        # 解析 From: ... To: ... Subject: ... Date: ...
        from_match = re.search(r"From:\s*(.*?)(?:\s+To:|$)", meta, re.DOTALL)
        if from_match:
            result["from"] = from_match.group(1).strip()
        to_match = re.search(r"To:\s*(.*?)(?:\s+Subject:|$)", meta, re.DOTALL)
        if to_match:
            result["to"] = to_match.group(1).strip()
        subject_match = re.search(r"Subject:\s*(.*?)(?:\s+Date:|$)", meta, re.DOTALL)
        if subject_match:
            result["subject"] = subject_match.group(1).strip()
        date_match = re.search(r"Date:\s*(.*?)$", meta, re.DOTALL)
        if date_match:
            result["date"] = date_match.group(1).strip()

        # 邮件正文：第一个 </p> 之后的所有内容
        p_end = desc.find("</p>")
        if p_end != -1:
            body = desc[p_end + 4 :].strip()
            if body:
                result["body"] = body

    return result


def get_emails(email: str) -> List[Email]:
    """获取邮件列表，通过 RSS XML 解析"""
    if not (email or "").strip():
        raise ValueError("eyepaste: empty email")
    e = email.strip()
    resp = tm_http.get(
        f"{BASE}/inbox/{e}.rss",
        headers=HEADERS,
        timeout=15,
    )
    resp.raise_for_status()
    content = resp.text
    if not content or not content.strip():
        return []

    try:
        root = ET.fromstring(content)
    except ET.ParseError:
        return []

    emails: List[Email] = []
    # RSS 2.0 结构: <rss><channel><item>...</item></channel></rss>
    channel_elem = root.find("channel")
    if channel_elem is None:
        return []

    for idx, item in enumerate(channel_elem.findall("item")):
        title = (item.findtext("title") or "").strip()
        desc = (item.findtext("description") or "").strip()

        parsed = _parse_description(desc)
        subject = parsed["subject"] or title
        from_addr = parsed["from"]
        to_addr = parsed["to"] or e
        date = parsed["date"]
        body_html = parsed["body"]

        # 从 HTML 正文提取纯文本
        text = ""
        if body_html:
            text = re.sub(r"<[^>]+>", "", body_html).strip()

        emails.append(
            Email(
                id=str(idx + 1),
                from_addr=from_addr,
                to=to_addr,
                subject=subject,
                text=text,
                html=body_html,
                date=date,
                is_read=False,
                attachments=[],
            )
        )

    return emails
