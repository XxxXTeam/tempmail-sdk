"""
24mail.chacuo.net 临时邮箱渠道 — http://24mail.chacuo.net

创建邮箱:
  POST /  form(x-www-form-urlencoded): data={name}@{domain}&type=refresh&arg=
  domains: chacuo.net, 027168.com
  响应: {"status":1,"info":"ok"}
  token 存储邮箱地址本身（后续请求以邮箱地址为参数）。
获取邮件:
  POST /  form: data={email}&type=refresh&arg=  （与创建相同格式）
  响应: {"status":1,"data":[{"list":[{MID, FROM, SUBJECT, CONTENT, TIME}]}]}
"""

import random
import string
from typing import Dict, List, Optional

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "24mail-chacuo"
BASE_URL = "http://24mail.chacuo.net/"
DEFAULT_DOMAIN = "chacuo.net"
DOMAINS = ["chacuo.net", "027168.com"]

_UA = (
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
)


def _random_local() -> str:
    chars = string.ascii_lowercase + string.digits
    return "sdk" + "".join(random.choice(chars) for _ in range(12))


def _pick_domain(preferred: Optional[str]) -> str:
    p = (preferred or "").strip().lstrip("@").lower()
    if p:
        for domain in DOMAINS:
            if domain.lower() == p:
                return domain
    return DEFAULT_DOMAIN


def _headers() -> Dict[str, str]:
    return {
        "User-Agent": _UA,
        "Accept": "application/json, text/javascript, */*; q=0.01",
        "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
        "X-Requested-With": "XMLHttpRequest",
        "Content-Type": "application/x-www-form-urlencoded; charset=UTF-8",
        "Origin": "http://24mail.chacuo.net",
        "Referer": "http://24mail.chacuo.net/",
    }


def _refresh(email: str) -> dict:
    """POST /  data={email}&type=refresh&arg=，返回 JSON 字典"""
    resp = tm_http.post(
        BASE_URL,
        headers=_headers(),
        data={"data": email, "type": "refresh", "arg": ""},
    )
    resp.raise_for_status()
    result = resp.json()
    if not isinstance(result, dict):
        raise RuntimeError("24mail-chacuo: 响应格式异常")
    return result


def generate_email(domain: Optional[str] = None) -> EmailInfo:
    """
    创建 24mail.chacuo.net 临时邮箱

    随机生成本地名并拼接域名，POST 刷新以在服务端建立收件箱。
    token 存储邮箱地址本身。
    """
    email = f"{_random_local()}@{_pick_domain(domain)}"
    result = _refresh(email)
    if result.get("status") != 1:
        raise RuntimeError(f"24mail-chacuo: 创建邮箱失败: {result.get('info')!r}")
    return EmailInfo(channel=CHANNEL, email=email, _token=email)


def get_emails(email: str, token: str) -> List[Email]:
    """
    获取 24mail.chacuo.net 邮件列表

    POST /  data={email}&type=refresh&arg=
    响应 data[0].list 为邮件数组。
    字段映射: MID=id, FROM=from, SUBJECT=subject, CONTENT=正文, TIME=date。
    """
    email = (email or token or "").strip()
    if not email:
        raise ValueError("24mail-chacuo: 邮箱地址为空")

    result = _refresh(email)
    data = result.get("data")
    if not isinstance(data, list) or not data:
        return []

    first = data[0]
    rows = first.get("list") if isinstance(first, dict) else None
    if not isinstance(rows, list) or not rows:
        return []

    out: List[Email] = []
    for item in rows:
        if not isinstance(item, dict):
            continue
        raw = {
            "id": item.get("MID", ""),
            "from": item.get("FROM", ""),
            "to": email,
            "subject": item.get("SUBJECT", ""),
            "content": item.get("CONTENT", ""),
            "date": item.get("TIME", ""),
        }
        out.append(normalize_email(raw, email))
    return out
