"""
altmails 渠道 — https://tempmail.altmails.com
Cookie Session + CSRF + REST JSON API。
创建邮箱: GET / 获取 session 和 CSRF → GET /random-email-address 获取随机邮箱
获取邮件: POST /fetch-emails/{email} → JSON 数组 + GET /view/{id} 获取邮件正文
域名: @altmails.com
"""

import json
import re
from typing import Dict, List, Optional

from ..config import get_config
from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "altmails"
BASE_URL = "https://tempmail.altmails.com"

# 从 HTML 中提取 CSRF token 的正则（inline script 中的 '_token': 'xxx'）
_CSRF_RE = re.compile(r"['\"]_token['\"]\s*:\s*['\"]([^'\"]+)['\"]")


def _get_ua() -> str:
    """获取 User-Agent，优先使用配置中的自定义 UA"""
    c = get_config()
    return (c.headers or {}).get("User-Agent") or (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    )


def _build_headers(extra: Optional[Dict[str, str]] = None) -> dict:
    """构造基础请求头"""
    h = {
        "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
        "Accept-Language": "en-US,en;q=0.9",
        "Cache-Control": "no-cache",
        "DNT": "1",
        "Pragma": "no-cache",
        "Upgrade-Insecure-Requests": "1",
        "User-Agent": _get_ua(),
    }
    if extra:
        h.update(extra)
    return h


def _encode_token(csrf: str, cookies: Dict[str, str]) -> str:
    """将 CSRF token 和 cookie 字典编码为 token 字符串（JSON）"""
    return json.dumps({"csrf": csrf, "cookies": cookies}, separators=(",", ":"))


def _decode_token(token: str) -> tuple:
    """从 token 字符串解码出 CSRF token 和 cookie 字典，返回 (csrf, cookies_dict)"""
    try:
        data = json.loads(token)
    except (json.JSONDecodeError, ValueError) as e:
        raise ValueError("altmails: token 格式无效") from e
    csrf = data.get("csrf", "")
    cookies = data.get("cookies", {})
    if not csrf or not isinstance(cookies, dict):
        raise ValueError("altmails: token 数据不完整")
    return csrf, cookies


def _cookies_to_str(cookies: Dict[str, str]) -> str:
    """将 cookie 字典序列化为 Cookie 头字符串"""
    return "; ".join(f"{k}={v}" for k, v in sorted(cookies.items()))


def generate_email() -> EmailInfo:
    """
    创建 altmails 临时邮箱

    流程:
    1. GET / 获取首页 HTML，保存 session cookie
    2. 从 HTML inline script 中提取 CSRF token: '_token': 'xxx'
    3. GET /random-email-address 获取随机分配的邮箱地址（纯文本）
    4. token 存储 JSON: {"csrf": "xxx", "cookies": {...}}
    """
    # 第一步: 获取首页，建立 session 并提取 CSRF
    r = tm_http.get(
        BASE_URL + "/",
        headers=_build_headers(),
    )
    r.raise_for_status()

    # 提取 CSRF token
    m = _CSRF_RE.search(r.text)
    if not m:
        raise RuntimeError("altmails: 无法从首页提取 CSRF token")
    csrf = m.group(1)

    # 收集 session cookie
    cookies = r.cookies.get_dict()
    cookie_str = _cookies_to_str(cookies)

    # 第二步: GET /random-email-address 获取随机邮箱地址
    r2 = tm_http.get(
        BASE_URL + "/random-email-address",
        headers=_build_headers(
            {
                "Cookie": cookie_str,
                "Referer": BASE_URL + "/",
            }
        ),
    )
    r2.raise_for_status()

    mailbox = (r2.text or "").strip()
    if not mailbox or "@" not in mailbox:
        raise RuntimeError(f"altmails: 返回的邮箱地址无效: {mailbox!r}")

    # 合并两次请求的 cookie
    merged_cookies = dict(cookies)
    merged_cookies.update(r2.cookies.get_dict())

    tok = _encode_token(csrf, merged_cookies)
    return EmailInfo(channel=CHANNEL, email=mailbox, _token=tok)


def get_emails(email: str, token: str) -> List[Email]:
    """
    获取 altmails 邮件列表

    流程:
    1. 从 token 解码出 CSRF token 和 cookie
    2. POST /fetch-emails/{email} 获取邮件列表（JSON 数组）
    3. 对每封邮件，GET /view/{id} 获取 HTML 正文
    4. 使用 normalize_email 标准化返回结果
    """
    if not token:
        raise ValueError("altmails: token 不能为空")

    addr = (email or "").strip()
    if not addr:
        raise ValueError("altmails: 邮箱地址不能为空")

    csrf, cookies = _decode_token(token)
    cookie_str = _cookies_to_str(cookies)

    # POST /fetch-emails/{email} 获取邮件列表
    r = tm_http.post(
        f"{BASE_URL}/fetch-emails/{addr}",
        headers=_build_headers(
            {
                "Accept": "application/json",
                "X-Requested-With": "XMLHttpRequest",
                "Cookie": cookie_str,
                "Referer": BASE_URL + "/",
                "Content-Type": "application/x-www-form-urlencoded",
            }
        ),
        data=f"_token={csrf}",
    )
    r.raise_for_status()

    messages = r.json()
    if not messages or not isinstance(messages, list):
        return []

    out: List[Email] = []
    for msg in messages:
        if not isinstance(msg, dict):
            continue

        mail_id = msg.get("id", "")

        # 尝试获取邮件 HTML 正文
        html_body = ""
        if mail_id:
            try:
                view_resp = tm_http.get(
                    f"{BASE_URL}/view/{mail_id}",
                    headers=_build_headers(
                        {
                            "Cookie": cookie_str,
                            "Referer": BASE_URL + "/",
                        }
                    ),
                )
                if view_resp.status_code == 200:
                    html_body = view_resp.text
            except Exception:
                # 单封邮件正文获取失败不影响其他邮件
                pass

        raw_email = {
            "id": str(mail_id),
            "from": msg.get("from", ""),
            "subject": msg.get("subject", ""),
            "html": html_body,
            "to": addr,
            "date": msg.get("date", ""),
        }
        out.append(normalize_email(raw_email, addr))

    return out
