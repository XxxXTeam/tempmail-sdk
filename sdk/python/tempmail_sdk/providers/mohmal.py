"""
mohmal.com 渠道 — https://www.mohmal.com
基于 HTML 解析的临时邮箱，使用 connect.sid session cookie 维持会话。
创建邮箱: GET /en/create/random -> 重定向到 /en/inbox，从页面提取 data-email 属性。
获取邮件: GET /en/inbox -> 解析 #inbox-table 表格行，逐条 GET /en/message/{id} 获取详情。
"""

import base64
import html
import json
import re
from typing import Dict, List, Optional, Tuple

import requests

from ..config import get_config
from .. import http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "mohmal"
ORIGIN = "https://www.mohmal.com"

# --- 正则表达式 ---

# 从收件箱页面提取 data-email 属性
_DATA_EMAIL_RE = re.compile(r'data-email="([^"]+)"')

# 解析收件箱表格行: 发件人、主题、时间、邮件链接
_INBOX_ROW_RE = re.compile(
    r'(?is)<tr[^>]*>\s*<td[^>]*>(.*?)</td>\s*<td[^>]*>(.*?)</td>\s*<td[^>]*>(.*?)</td>'
)

# 提取邮件详情链接中的 ID
_MESSAGE_LINK_RE = re.compile(r'/en/message/(\d+)')

# 提取邮件正文区域
_MESSAGE_BODY_RE = re.compile(r'(?is)<div[^>]*class="[^"]*mail-content[^"]*"[^>]*>([\s\S]*?)</div>')

# 备选正文提取（如果 mail-content 找不到，尝试 message-body）
_MESSAGE_BODY_ALT_RE = re.compile(r'(?is)<div[^>]*class="[^"]*message-body[^"]*"[^>]*>([\s\S]*?)</div>')

# 提取邮件详情页中的发件人
_DETAIL_FROM_RE = re.compile(r'(?is)<span[^>]*class="[^"]*from[^"]*"[^>]*>([\s\S]*?)</span>')

# 提取邮件详情页中的主题
_DETAIL_SUBJECT_RE = re.compile(r'(?is)<span[^>]*class="[^"]*subject[^"]*"[^>]*>([\s\S]*?)</span>')

# 提取邮件详情页中的日期
_DETAIL_DATE_RE = re.compile(r'(?is)<span[^>]*class="[^"]*date[^"]*"[^>]*>([\s\S]*?)</span>')

# 从发件人字段提取邮箱地址
_FROM_ADDR_RE = re.compile(
    r"[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}"
)

# 清除 HTML 标签
_TAG_RE = re.compile(r"<[^>]+>")

# token 前缀
TOK_PREFIX = "moh1:"

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


def _page_headers(referer: str, cookie: str = "") -> dict:
    """构造页面请求头"""
    h = {
        **_DEFAULT_HEADERS,
        "User-Agent": _get_ua(),
        "Referer": referer,
    }
    if cookie:
        h["Cookie"] = cookie
    return h


def _strip_tags(s: str) -> str:
    """清除 HTML 标签并反转义"""
    return html.unescape(_TAG_RE.sub("", s)).strip()


def _encode_token(cookie_hdr: str) -> str:
    """将 session cookie 编码为 token"""
    raw = json.dumps({"c": cookie_hdr}, separators=(",", ":")).encode("utf-8")
    return TOK_PREFIX + base64.standard_b64encode(raw).decode("ascii")


def _decode_token(tok: str) -> str:
    """从 token 解码出 session cookie"""
    if not tok.startswith(TOK_PREFIX):
        raise ValueError("mohmal: invalid session token")
    try:
        data = base64.standard_b64decode(tok[len(TOK_PREFIX):].encode("ascii"))
        o = json.loads(data.decode("utf-8"))
    except (json.JSONDecodeError, ValueError) as e:
        raise ValueError("mohmal: invalid session token") from e
    c = (o.get("c") or "").strip()
    if not c:
        raise ValueError("mohmal: invalid session token (empty cookie)")
    return c


def _parse_cookie_map(hdr: str) -> Dict[str, str]:
    """解析 Cookie 头字符串为字典"""
    m: Dict[str, str] = {}
    for part in hdr.split(";"):
        part = part.strip()
        if not part or "=" not in part:
            continue
        k, _, v = part.partition("=")
        k, v = k.strip(), v.strip()
        if k:
            m[k] = v
    return m


def _merge_cookie_hdr(prev: str, resp: requests.Response) -> str:
    """合并已有 cookie 和响应中的 Set-Cookie"""
    d = _parse_cookie_map(prev)
    d.update(resp.cookies.get_dict())
    return "; ".join(f"{k}={d[k]}" for k in sorted(d.keys()))


def generate_email(**kwargs) -> EmailInfo:
    """
    创建 mohmal 临时邮箱

    流程:
    1. GET /en/create/random，服务器设置 connect.sid cookie 并重定向到 /en/inbox
    2. 从 /en/inbox 页面 HTML 中提取 data-email 属性获取邮箱地址
    """
    create_url = f"{ORIGIN}/en/create/random"
    inbox_url = f"{ORIGIN}/en/inbox"

    # 第一步: GET /en/create/random，不跟随重定向以捕获 session cookie
    r1 = http.get(
        create_url,
        headers=_page_headers(ORIGIN),
        allow_redirects=False,
    )
    cookie_hdr = _merge_cookie_hdr("", r1)

    # 如果返回重定向，手动跟随
    if r1.status_code in (301, 302, 303, 307, 308):
        r2 = http.get(
            inbox_url,
            headers=_page_headers(create_url, cookie_hdr),
        )
        r2.raise_for_status()
        cookie_hdr = _merge_cookie_hdr(cookie_hdr, r2)
        page_html = r2.text
    else:
        # 没有重定向的情况，直接使用响应
        r1.raise_for_status()
        cookie_hdr = _merge_cookie_hdr(cookie_hdr, r1)
        # 仍然需要访问 inbox 获取邮箱地址
        r2 = http.get(
            inbox_url,
            headers=_page_headers(create_url, cookie_hdr),
        )
        r2.raise_for_status()
        cookie_hdr = _merge_cookie_hdr(cookie_hdr, r2)
        page_html = r2.text

    # 验证 connect.sid cookie 存在
    cookies = _parse_cookie_map(cookie_hdr)
    if "connect.sid" not in cookies:
        raise RuntimeError("mohmal: missing connect.sid session cookie")

    # 从页面提取邮箱地址
    m = _DATA_EMAIL_RE.search(page_html)
    if not m:
        raise RuntimeError("mohmal: unable to extract email address from inbox page")
    email = html.unescape(m.group(1)).strip()
    if not email or "@" not in email:
        raise RuntimeError(f"mohmal: invalid email address: {email}")

    tok = _encode_token(cookie_hdr)
    return EmailInfo(channel=CHANNEL, email=email, _token=tok)


def get_emails(email: str, token: str, **kwargs) -> List[Email]:
    """
    获取 mohmal 邮件列表

    流程:
    1. GET /en/inbox 获取收件箱页面
    2. 解析表格行提取邮件摘要（发件人、主题、时间）和邮件 ID
    3. 对每封邮件 GET /en/message/{id} 获取完整内容
    """
    cookie_hdr = _decode_token(token)
    inbox_url = f"{ORIGIN}/en/inbox"

    # 获取收件箱页面
    r = http.get(
        inbox_url,
        headers=_page_headers(ORIGIN, cookie_hdr),
    )
    r.raise_for_status()
    inbox_html = r.text

    # 提取所有邮件链接中的 ID
    message_ids = _MESSAGE_LINK_RE.findall(inbox_html)
    if not message_ids:
        return []

    # 去重并保持顺序
    seen = set()
    unique_ids: List[str] = []
    for mid in message_ids:
        if mid not in seen:
            seen.add(mid)
            unique_ids.append(mid)

    # 逐条获取邮件详情
    result: List[Email] = []
    for mid in unique_ids:
        detail_url = f"{ORIGIN}/en/message/{mid}"
        rd = http.get(
            detail_url,
            headers=_page_headers(inbox_url, cookie_hdr),
        )
        if rd.status_code != 200:
            continue
        raw = _parse_message_detail(rd.text, mid, email)
        result.append(normalize_email(raw, email))

    return result


def _parse_message_detail(page: str, mid: str, recipient: str) -> dict:
    """从邮件详情页面解析邮件数据"""
    # 提取发件人
    from_addr = ""
    fm = _DETAIL_FROM_RE.search(page)
    if fm:
        from_raw = html.unescape(fm.group(1))
        # 尝试提取纯邮箱地址
        em = _FROM_ADDR_RE.search(from_raw)
        from_addr = em.group(0) if em else _strip_tags(from_raw)

    # 提取主题
    subject = ""
    sm = _DETAIL_SUBJECT_RE.search(page)
    if sm:
        subject = _strip_tags(sm.group(1))

    # 提取日期
    date = ""
    dm = _DETAIL_DATE_RE.search(page)
    if dm:
        date = _strip_tags(dm.group(1))

    # 提取正文 HTML
    body_html = ""
    bm = _MESSAGE_BODY_RE.search(page)
    if not bm:
        bm = _MESSAGE_BODY_ALT_RE.search(page)
    if bm:
        body_html = bm.group(1).strip()

    return {
        "id": mid,
        "from": from_addr,
        "to": recipient,
        "subject": subject,
        "date": date,
        "html": body_html,
    }
