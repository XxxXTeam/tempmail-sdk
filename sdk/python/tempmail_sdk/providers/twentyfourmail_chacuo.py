"""
24mail-chacuo — http://24mail.chacuo.net
POST 注册/刷新同一接口，HTTP only。
"""

import random
import string
from typing import List

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "24mail-chacuo"
BASE_URL = "http://24mail.chacuo.net"
DOMAINS = ["chacuo.net", "027168.com"]

HEADERS = {
    "Accept": "application/json, text/javascript, */*; q=0.01",
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "Content-Type": "application/x-www-form-urlencoded; charset=UTF-8",
    "Origin": BASE_URL,
    "Referer": f"{BASE_URL}/",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
    "x-requested-with": "XMLHttpRequest",
}


def _random_local(length: int) -> str:
    """生成随机字母数字用户名"""
    chars = string.ascii_lowercase + string.digits
    return "".join(random.choice(chars) for _ in range(length))


def generate_email() -> EmailInfo:
    """创建 24mail-chacuo 临时邮箱"""
    name = _random_local(10)
    domain = random.choice(DOMAINS)

    body = f"data={name}%40{domain}&type=refresh&arg="
    r = tm_http.post(f"{BASE_URL}/", headers=HEADERS, data=body, timeout=15)
    r.raise_for_status()

    data = r.json()
    if data.get("status") != 1:
        raise RuntimeError(
            f"24mail-chacuo: 创建失败 status={data.get('status')} info={data.get('info')}"
        )

    email_addr = f"{name}@{domain}"
    return EmailInfo(channel=CHANNEL, email=email_addr, _token=email_addr)


def get_emails(email: str, _token: str = "") -> List[Email]:
    """获取 24mail-chacuo 邮件列表"""
    at_idx = email.find("@")
    if at_idx > 0:
        name = email[:at_idx]
        domain = email[at_idx + 1:]
    else:
        name = email
        domain = DOMAINS[0]

    body = f"data={name}%40{domain}&type=refresh&arg="
    r = tm_http.post(f"{BASE_URL}/", headers=HEADERS, data=body, timeout=15)
    r.raise_for_status()

    data = r.json()
    if data.get("status") != 1 or not data.get("data"):
        return []

    entry = data["data"][0]
    mail_list = entry.get("list") or []

    out: List[Email] = []
    for item in mail_list:
        flat = {
            "id": item.get("MID", ""),
            "from": item.get("FROM", ""),
            "to": email,
            "subject": item.get("SUBJECT", ""),
            "body": item.get("CONTENT", ""),
            "date": item.get("TIME", ""),
            "isRead": False,
        }
        out.append(normalize_email(flat, email))
    return out
