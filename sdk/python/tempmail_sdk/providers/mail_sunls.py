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


def _fetch_detail(mail_id: str) -> dict | None:
    """
    通过详情接口获取单封邮件完整正文
    GET /api/fetch/{id}
    失败时返回 None，调用方回退到列表数据
    """
    mid = (mail_id or "").strip()
    if not mid:
        return None
    try:
        resp = tm_http.get(
            f"{BASE}/api/fetch/{quote(mid, safe='')}",
            headers=HEADERS,
            timeout=15,
        )
        if resp.status_code < 200 or resp.status_code >= 300:
            return None
        data = resp.json()
        if isinstance(data, dict):
            return data
    except Exception:  # noqa: BLE001
        return None
    return None


def _extract_id(row: dict) -> str:
    """从列表条目提取邮件 ID（支持多种字段和类型）"""
    for key in ("id", "_id", "mail_id", "messageId", "message_id"):
        v = row.get(key)
        if v is None:
            continue
        if isinstance(v, str):
            s = v.strip()
            if s:
                return s
        elif isinstance(v, (int, float)) and not isinstance(v, bool):
            return str(int(v))
    return ""


def get_emails(email: str) -> List[Email]:
    """
    获取邮件列表
    1. GET /api/fetch?to={email} 拉取列表元数据
    2. 对每封邮件 GET /api/fetch/{id} 拉取详情（含完整 text/html）
    3. 详情失败时保留列表字段作为回退
    """
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
        mail_id = _extract_id(raw)
        merged = dict(raw)
        # 无条件调用详情接口，用详情字段覆盖列表字段
        if mail_id:
            detail = _fetch_detail(mail_id)
            if detail:
                for k, v in detail.items():
                    if v is not None:
                        merged[k] = v
        out.append(normalize_email(merged, addr))
    return out
