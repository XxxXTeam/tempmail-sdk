"""
Haribu 渠道 — https://haribu.net
Tempail 类模式的临时邮箱服务，需要 session cookies 维持会话
创建邮箱：GET https://haribu.net → 从 HTML 中提取 <input id="eposta_adres" value="xxx@yevme.com">
获取邮件：GET https://haribu.net → 解析页面中 <li class="mail"> 条目
辅助 API：GET https://haribu.net/en/api-kontrol/ → 检查新邮件
"""

import base64
import json
import re
from html import unescape
from typing import List, Optional

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "haribu"
BASE = "https://haribu.net"

# token 前缀，用于区分不同渠道的 token 格式
_TOK_PREFIX = "haribu1:"

HEADERS = {
    "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
    "Cache-Control": "no-cache",
    "DNT": "1",
    "Pragma": "no-cache",
    "Upgrade-Insecure-Requests": "1",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
}

# 匹配邮箱地址输入框：<input id="eposta_adres" value="xxx@yyy.com">
_EMAIL_INPUT_RE = re.compile(
    r'(?i)<input[^>]+id\s*=\s*["\']eposta_adres["\'][^>]+value\s*=\s*["\']([^"\']+)["\']'
)
# 备选匹配顺序（value 在 id 前面的情况）
_EMAIL_INPUT_RE2 = re.compile(
    r'(?i)<input[^>]+value\s*=\s*["\']([^"\']+@[^"\']+)["\'][^>]+id\s*=\s*["\']eposta_adres["\']'
)
# 匹配邮件列表条目 <li class="mail"> ... </li>
_MAIL_ITEM_RE = re.compile(
    r'(?is)<li\s+class\s*=\s*["\']mail["\'][^>]*>([\s\S]*?)</li>'
)
# 匹配发件人
_FROM_RE = re.compile(
    r'(?is)<span\s+class\s*=\s*["\']mail_gonderen["\'][^>]*>([\s\S]*?)</span>'
)
# 匹配邮件主题
_SUBJECT_RE = re.compile(
    r'(?is)<span\s+class\s*=\s*["\']mail_konu["\'][^>]*>([\s\S]*?)</span>'
)
# 匹配邮件时间
_DATE_RE = re.compile(
    r'(?is)<span\s+class\s*=\s*["\']mail_zaman["\'][^>]*>([\s\S]*?)</span>'
)
# 匹配邮件详情链接
_MAIL_LINK_RE = re.compile(
    r'(?i)href\s*=\s*["\']([^"\']*(?:mail|read|view)[^"\']*)["\']'
)
# 匹配邮件正文容器
_BODY_OPEN_RE = re.compile(
    r'(?is)<div\s+(?:id|class)\s*=\s*["\'](?:mail_icerik|icerik|mail-content|message-body)["\'][^>]*>'
)


def _extract_body_html(page: str) -> str:
    """使用栈式深度匹配提取正文 div 的完整内部 HTML，
    避免非贪婪正则在嵌套 div 时截断正文。
    """
    m = _BODY_OPEN_RE.search(page)
    if not m:
        return ""
    start = m.end()
    pos = start
    depth = 1
    while pos < len(page) and depth > 0:
        next_open = page.find("<div", pos)
        next_close = page.find("</div>", pos)
        if next_close == -1:
            break
        if next_open != -1 and next_open < next_close:
            depth += 1
            pos = next_open + 4
        else:
            depth -= 1
            if depth == 0:
                return page[start:next_close].strip()
            pos = next_close + 6
    return ""


def _strip_tags(html_str: str) -> str:
    """移除 HTML 标签，返回纯文本"""
    return re.sub(r"<[^>]+>", " ", html_str).strip()


def _parse_cookies(resp) -> dict:
    """从响应中提取 cookies 为字典"""
    cookies = {}
    if hasattr(resp, "cookies"):
        for k, v in resp.cookies.items():
            cookies[k] = v
    return cookies


def _cookies_to_header(cookies: dict) -> str:
    """将 cookie 字典序列化为 Cookie 头字符串"""
    return "; ".join(f"{k}={v}" for k in sorted(cookies) for v in [cookies[k]])


def _merge_cookies(prev: str, resp) -> str:
    """合并已有 cookie 和响应中的新 cookie"""
    # 解析已有 cookie
    existing = {}
    if prev:
        for part in prev.split(";"):
            part = part.strip()
            if "=" in part:
                k, v = part.split("=", 1)
                k, v = k.strip(), v.strip()
                if k:
                    existing[k] = v
    # 合并新 cookie
    new_cookies = _parse_cookies(resp)
    existing.update(new_cookies)
    return _cookies_to_header(existing)


def _encode_sess(cookie_hdr: str) -> str:
    """将会话信息编码为 token 字符串"""
    payload = json.dumps({"c": cookie_hdr}).encode("utf-8")
    return _TOK_PREFIX + base64.b64encode(payload).decode("ascii")


def _decode_sess(token: str) -> str:
    """从 token 字符串解码 cookie 头，返回 cookie_hdr"""
    if not token.startswith(_TOK_PREFIX):
        raise ValueError("haribu: 无效的会话令牌")
    raw = token[len(_TOK_PREFIX) :]
    try:
        decoded = base64.b64decode(raw)
        data = json.loads(decoded)
    except Exception:
        raise ValueError("haribu: 无效的会话令牌")
    cookie_hdr = data.get("c", "")
    if not cookie_hdr:
        raise ValueError("haribu: 会话令牌中缺少 cookie")
    return cookie_hdr


def _extract_email(html_str: str) -> str:
    """从 HTML 中提取邮箱地址（匹配 id="eposta_adres" 的 input 元素）"""
    # 尝试标准顺序：id 在 value 前
    m = _EMAIL_INPUT_RE.search(html_str)
    if m:
        addr = unescape(m.group(1)).strip()
        if addr and "@" in addr:
            return addr
    # 备选顺序：value 在 id 前
    m = _EMAIL_INPUT_RE2.search(html_str)
    if m:
        addr = unescape(m.group(1)).strip()
        if addr and "@" in addr:
            return addr
    raise RuntimeError("haribu: 未找到邮箱地址（eposta_adres）")


def generate_email(channel: str = CHANNEL) -> EmailInfo:
    """
    创建 haribu 临时邮箱
    流程：GET haribu.net → 获取 session cookie → 从 HTML 提取邮箱地址
    """
    resp = tm_http.get(BASE, headers=HEADERS, timeout=15)
    resp.raise_for_status()
    html_str = resp.text
    if not html_str:
        raise RuntimeError("haribu: 首页响应为空")

    # 从 HTML 中提取邮箱地址
    email_addr = _extract_email(html_str)

    # 收集 session cookie 并编码为 token
    cookie_hdr = _merge_cookies("", resp)
    token = _encode_sess(cookie_hdr)

    return EmailInfo(channel=channel, email=email_addr, _token=token)


def _fetch_detail(detail_url: str, cookie_hdr: str) -> str:
    """获取单封邮件的详情页正文"""
    try:
        detail_headers = dict(HEADERS)
        detail_headers["Cookie"] = cookie_hdr
        detail_headers["Referer"] = BASE
        resp = tm_http.get(detail_url, headers=detail_headers, timeout=15)
        if resp.status_code < 200 or resp.status_code >= 300:
            return ""
        html_str = resp.text or ""
        body = _extract_body_html(html_str)
        if body:
            return body
    except Exception:
        pass
    return ""


def get_emails(token: str, email: str) -> List[Email]:
    """
    获取 haribu 邮件列表
    流程：先调用 api-kontrol 检查新邮件 → GET 首页解析邮件列表 → 提取各封邮件详情
    """
    if not (email or "").strip():
        raise ValueError("haribu: 邮箱地址为空")
    e = email.strip()

    cookie_hdr = _decode_sess(token)

    # 调用 kontrol API 触发新邮件检查
    try:
        kontrol_headers = dict(HEADERS)
        kontrol_headers["Cookie"] = cookie_hdr
        kontrol_headers["Referer"] = BASE
        kontrol_headers["X-Requested-With"] = "XMLHttpRequest"
        tm_http.get(f"{BASE}/en/api-kontrol/", headers=kontrol_headers, timeout=15)
    except Exception:
        pass

    # GET 首页获取收件箱页面
    inbox_headers = dict(HEADERS)
    inbox_headers["Cookie"] = cookie_hdr
    inbox_headers["Referer"] = BASE
    resp = tm_http.get(BASE, headers=inbox_headers, timeout=15)
    resp.raise_for_status()
    html_str = resp.text or ""

    # 解析邮件列表中的 <li class="mail"> 条目
    items = _MAIL_ITEM_RE.findall(html_str)
    if not items:
        return []

    emails: List[Email] = []
    for idx, content in enumerate(items):
        raw: dict = {
            "id": f"haribu-{idx}",
            "to": e,
        }

        # 提取发件人
        fm = _FROM_RE.search(content)
        if fm:
            raw["from"] = unescape(_strip_tags(fm.group(1))).strip()

        # 提取主题
        sm = _SUBJECT_RE.search(content)
        if sm:
            raw["subject"] = unescape(_strip_tags(sm.group(1))).strip()

        # 提取时间
        dm = _DATE_RE.search(content)
        if dm:
            raw["date"] = unescape(_strip_tags(dm.group(1))).strip()

        # 尝试获取邮件正文：如果有详情链接则请求详情页
        lm = _MAIL_LINK_RE.search(content)
        if lm:
            detail_url = lm.group(1)
            if not detail_url.startswith("http"):
                detail_url = BASE + "/" + detail_url.lstrip("/")
            html_body = _fetch_detail(detail_url, cookie_hdr)
            if html_body:
                raw["html"] = html_body

        emails.append(normalize_email(raw, e))

    return emails
