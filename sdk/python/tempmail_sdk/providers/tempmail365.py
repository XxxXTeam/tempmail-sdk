"""
Tempmail365 渠道 — https://tempmail365.cn
"""

import random
import re
import string
from datetime import datetime, timezone
from typing import List, Optional

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "tempmail365"
BASE = "https://tempmail365.cn/tempemail.php"

# 可用域名列表（后备）
FALLBACK_DOMAINS = ["fengyou.cc", "shop345.com", "nutemail.com", "qvrf.cn"]

HEADERS = {
    "Accept": "application/json, text/plain, */*",
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
    "Cache-Control": "no-cache",
    "DNT": "1",
    "Pragma": "no-cache",
    "Referer": "https://tempmail365.cn/",
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


def _fetch_domains() -> List[str]:
    """从 API 获取可用域名列表，失败时使用后备域名"""
    try:
        resp = tm_http.get(f"{BASE}?action=get_config", headers=HEADERS, timeout=15)
        resp.raise_for_status()
        data = resp.json()
        if not isinstance(data, dict):
            return FALLBACK_DOMAINS[:]
        raw = data.get("domains")
        if not isinstance(raw, list) or not raw:
            return FALLBACK_DOMAINS[:]
        out: List[str] = []
        for d in raw:
            if isinstance(d, str) and d.strip():
                out.append(d.strip())
        return out if out else FALLBACK_DOMAINS[:]
    except Exception:
        return FALLBACK_DOMAINS[:]


def _random_username(length: int = 8) -> str:
    """生成随机用户名（8位字母数字）"""
    return "".join(random.choices(string.ascii_lowercase + string.digits, k=length))


def _extract_sender(content: str) -> str:
    """从 HTML 邮件内容中提取发件人"""
    # 尝试匹配 "发件人:" 或 "From:" 后面的内容
    m = re.search(r"(?:发件人|From)\s*[:：]\s*(.+?)(?:<br|</|<p|\n|\r)", content, re.IGNORECASE)
    if m:
        return m.group(1).strip()
    return ""


def _extract_subject(content: str) -> str:
    """从 HTML 邮件内容中提取主题"""
    # 尝试匹配 "主题:" 或 "Subject:" 后面的内容
    m = re.search(r"(?:主题|Subject)\s*[:：]\s*(.+?)(?:<br|</|<p|\n|\r)", content, re.IGNORECASE)
    if m:
        return m.group(1).strip()
    return ""


def generate_email(domain: Optional[str] = None, channel: str = CHANNEL) -> EmailInfo:
    """创建临时邮箱"""
    domains = _fetch_domains()
    if not domains:
        raise RuntimeError("tempmail365: 无可用域名")

    # 选择域名
    if domain and domain.strip():
        d = domain.strip().lower()
        matched = [x for x in domains if x.lower() == d]
        if not matched:
            raise ValueError(f"tempmail365: 域名不可用: {d}")
        selected = matched[0]
    else:
        selected = random.choice(domains)

    # 生成随机用户名
    username = _random_username()
    addr = f"{username}@{selected}"

    # 创建邮箱
    from urllib.parse import quote
    url = f"{BASE}?action=create_email&email={quote(addr, safe='')}&domain={quote(selected, safe='')}"
    resp = tm_http.get(url, headers=HEADERS, timeout=15)
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, dict) or not data.get("success"):
        raise RuntimeError("tempmail365: 创建邮箱失败")

    return EmailInfo(channel=channel, email=addr)


def get_emails(email: str) -> List[Email]:
    """获取邮件列表"""
    from urllib.parse import quote

    if not (email or "").strip():
        raise ValueError("tempmail365: 邮箱地址为空")
    e = email.strip()

    resp = tm_http.get(
        f"{BASE}?action=fetch_mail&email={quote(e, safe='')}",
        headers=HEADERS,
        timeout=15,
    )
    resp.raise_for_status()
    data = resp.json()
    if not isinstance(data, dict):
        return []

    content = data.get("content", "")
    if not content or not isinstance(content, str):
        return []

    # "无邮件" 表示没有邮件
    if content.strip() == "无邮件":
        return []

    # 从 HTML 内容中提取发件人和主题
    sender = _extract_sender(content)
    subject = _extract_subject(content)

    # 构造邮件对象并通过 normalize 处理
    raw = {
        "from": sender,
        "subject": subject,
        "body": content,
        "date": datetime.now(timezone.utc).isoformat(),
    }
    return [normalize_email(raw, e)]
