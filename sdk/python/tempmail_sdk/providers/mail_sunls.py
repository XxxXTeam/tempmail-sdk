"""
Mail Sunls 渠道 — https://mail.sunls.de
无需 token，无需 session
创建邮箱：从 /api/domain 获取域名列表，本地随机生成地址
获取邮件：GET /api/fetch?to={email}
"""

import random
import string
from typing import List
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "mail-sunls"
BASE = "https://mail.sunls.de"

HEADERS = {
    "Accept": "application/json, text/plain, */*",
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
    "Cache-Control": "no-cache",
    "DNT": "1",
    "Pragma": "no-cache",
    "Referer": "https://mail.sunls.de/",
    "sec-ch-ua": '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
    "sec-ch-ua-mobile": "?0",
    "sec-ch-ua-platform": '"Windows"',
    "sec-fetch-dest": "empty",
    "sec-fetch-mode": "cors",
    "sec-fetch-site": "same-origin",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
}


def _random_local(length: int = 10) -> str:
    """生成随机本地部分"""
    return "".join(random.choices(string.ascii_lowercase + string.digits, k=length))


def _fetch_domains() -> List[str]:
    """从 API 获取可用域名列表"""
    resp = tm_http.get(f"{BASE}/api/domain", headers=HEADERS, timeout=15)
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, list):
        raise ValueError("mail-sunls: 域名列表响应格式无效")
    out: List[str] = []
    for d in data:
        if isinstance(d, str) and d.strip():
            out.append(d.strip())
    if not out:
        raise ValueError("mail-sunls: 无可用域名")
    return out


def generate_email() -> EmailInfo:
    """创建临时邮箱：获取域名列表后随机生成地址，无需 API 调用创建"""
    domains = _fetch_domains()
    domain = random.choice(domains)
    local = _random_local(10)
    email = f"{local}@{domain}"
    return EmailInfo(channel=CHANNEL, email=email)


def get_emails(email: str) -> List[Email]:
    """获取指定邮箱的邮件列表"""
    if not (email or "").strip():
        raise ValueError("mail-sunls: 邮箱地址不能为空")
    addr = email.strip()
    resp = tm_http.get(
        f"{BASE}/api/fetch?to={quote(addr, safe='')}",
        headers=HEADERS,
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, list):
        return []
    out: List[Email] = []
    for raw in data:
        if not isinstance(raw, dict):
            continue
        out.append(normalize_email(raw, addr))
    return out
