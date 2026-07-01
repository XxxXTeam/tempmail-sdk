"""
minuteinbox.com 渠道 — https://www.minuteinbox.com
Cookie Session + CSRF + REST JSON API。
创建邮箱: GET / 获取 session cookie 和 CSRF token，再 GET /index/index?csrf_token={csrf} 获取邮箱地址。
获取邮件: GET /index/refresh 获取邮件列表，GET /email/id/{id} 获取邮件正文。
域名: @minafter.com（由 API 返回，可能变化）
"""

import json
import re
from typing import Dict, List

import requests

from ..config import get_config
from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "minuteinbox"
ORIGIN = "https://www.minuteinbox.com"

# 从首页 HTML 中提取 CSRF token 的正则
_CSRF_RE = re.compile(r'CSRF\s*=\s*"([^"]+)"')

# 默认请求头
_DEFAULT_HEADERS = {
    "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
    "Accept-Language": "en-US,en;q=0.9",
    "Cache-Control": "no-cache",
    "DNT": "1",
    "Pragma": "no-cache",
    "Upgrade-Insecure-Requests": "1",
}


def _get_ua() -> str:
    """获取 User-Agent，优先使用配置中的自定义 UA"""
    c = get_config()
    return (c.headers or {}).get("User-Agent") or (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    )


def _build_headers(referer: str = "", cookie: str = "", accept: str = "") -> dict:
    """构造请求头"""
    h = {
        **_DEFAULT_HEADERS,
        "User-Agent": _get_ua(),
    }
    if referer:
        h["Referer"] = referer
    if cookie:
        h["Cookie"] = cookie
    if accept:
        h["Accept"] = accept
    return h


def _cookies_to_str(cookies: Dict[str, str]) -> str:
    """将 cookie 字典序列化为 Cookie 头字符串"""
    return "; ".join(f"{k}={v}" for k, v in sorted(cookies.items()))


def _merge_cookies(prev: Dict[str, str], resp: requests.Response) -> Dict[str, str]:
    """合并已有 cookie 字典和响应中的 Set-Cookie"""
    merged = dict(prev)
    merged.update(resp.cookies.get_dict())
    return merged


def _encode_token(csrf: str, cookies: Dict[str, str]) -> str:
    """将 CSRF token 和 cookie 字典编码为 token 字符串（JSON）"""
    return json.dumps({"csrf": csrf, "cookies": cookies}, separators=(",", ":"))


def _decode_token(token: str) -> tuple:
    """从 token 字符串解码出 CSRF token 和 cookie 字典，返回 (csrf, cookies_dict)"""
    try:
        data = json.loads(token)
    except (json.JSONDecodeError, ValueError) as e:
        raise ValueError("minuteinbox: token 格式无效") from e
    csrf = data.get("csrf", "")
    cookies = data.get("cookies", {})
    if not csrf or not isinstance(cookies, dict):
        raise ValueError("minuteinbox: token 数据不完整")
    return csrf, cookies


def generate_email(channel: str = CHANNEL) -> EmailInfo:
    """
    创建 minuteinbox 临时邮箱

    流程:
    1. GET / 获取首页 HTML，保存 session cookie，从 HTML 中正则提取 CSRF token
    2. GET /index/index?csrf_token={csrf} 创建邮箱，返回 {"email":"user@minafter.com"}
    3. token 存储 JSON: {"csrf": "xxx", "cookies": {...}}
    """
    # 第一步: 访问首页获取 session cookie 和 CSRF token
    r1 = tm_http.get(
        ORIGIN + "/",
        headers=_build_headers(),
    )
    r1.raise_for_status()

    # 收集 session cookie
    cookies = r1.cookies.get_dict()

    # 从 HTML 中提取 CSRF token
    m = _CSRF_RE.search(r1.text)
    if not m:
        raise RuntimeError("minuteinbox: 无法从首页 HTML 提取 CSRF token")
    csrf = m.group(1)

    # 第二步: 使用 CSRF token 创建邮箱
    cookie_str = _cookies_to_str(cookies)
    r2 = tm_http.get(
        f"{ORIGIN}/index/index",
        headers=_build_headers(
            referer=f"{ORIGIN}/",
            cookie=cookie_str,
            accept="application/json, text/javascript, */*; q=0.01",
        ),
        params={"csrf_token": csrf},
    )
    r2.raise_for_status()

    # 合并可能的新 cookie
    cookies = _merge_cookies(cookies, r2)

    # 解析邮箱地址
    data = r2.json()
    email_addr = data.get("email", "").strip()
    if not email_addr or "@" not in email_addr:
        raise RuntimeError(f"minuteinbox: 返回的邮箱地址无效: {data!r}")

    tok = _encode_token(csrf, cookies)
    return EmailInfo(channel=channel, email=email_addr, _token=tok)


def get_emails(email: str, token: str) -> List[Email]:
    """
    获取 minuteinbox 邮件列表

    流程:
    1. 从 token 解码出 CSRF 和 cookie
    2. GET /index/refresh 获取邮件列表（空收件箱返回 0）
    3. 遍历列表，GET /email/id/{id} 获取每封邮件的 HTML 正文
    4. 使用 normalize_email 标准化返回结果

    字段说明（捷克语）:
    - predmet: 主题 (subject)
    - od: 发件人 (from)
    - id: 邮件 ID
    - kdy: 时间 (when)
    - precteno: 已读状态 ("new" 表示未读，"precteno" 表示已读)
    """
    if not token:
        raise ValueError("minuteinbox: token 不能为空")

    addr = (email or "").strip()
    if not addr:
        raise ValueError("minuteinbox: 邮箱地址不能为空")

    csrf, cookies = _decode_token(token)
    cookie_str = _cookies_to_str(cookies)

    # 获取邮件列表
    r = tm_http.get(
        f"{ORIGIN}/index/refresh",
        headers=_build_headers(
            referer=f"{ORIGIN}/",
            cookie=cookie_str,
            accept="application/json, text/javascript, */*; q=0.01",
        ),
    )
    r.raise_for_status()

    # 空收件箱返回数字 0
    raw = r.json()
    if not isinstance(raw, list):
        return []

    out: List[Email] = []
    for item in raw:
        if not isinstance(item, dict):
            continue

        mail_id = item.get("id")
        if mail_id is None:
            continue

        # 获取邮件 HTML 正文
        body_html = ""
        detail_resp = tm_http.get(
            f"{ORIGIN}/email/id/{mail_id}",
            headers=_build_headers(
                referer=f"{ORIGIN}/",
                cookie=cookie_str,
            ),
        )
        if detail_resp.status_code == 200:
            body_html = detail_resp.text.strip()

        # 判断已读状态: "new" 表示未读，其他值表示已读
        is_read = item.get("precteno", "") != "new"

        raw_email = {
            "id": str(mail_id),
            "from": item.get("od", ""),
            "to": addr,
            "subject": item.get("predmet", ""),
            "html": body_html,
            "date": item.get("kdy", ""),
            "is_read": is_read,
        }
        out.append(normalize_email(raw_email, addr))

    return out
