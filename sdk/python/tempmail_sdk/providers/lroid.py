"""
Lroid 渠道 — https://lroid.com
HTML 解析模式，使用 session cookies 维持邮箱身份
邮箱地址在首次访问时由服务端自动分配，域名: yevme.com
"""

import json
import re
from typing import List

import requests

from ..config import get_config
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "lroid"
BASE_URL = "https://lroid.com"

# 从 HTML 中提取 <input id="eposta_adres" value="xxx@yevme.com">
EMAIL_RE = re.compile(
    r'<input[^>]+id=["\']eposta_adres["\'][^>]+value=["\']([^"\']+@[^"\']+)["\']',
    re.I,
)
# 备选顺序：value 在 id 之前的情况
EMAIL_RE_ALT = re.compile(
    r'<input[^>]+value=["\']([^"\']+@[^"\']+)["\'][^>]+id=["\']eposta_adres["\']',
    re.I,
)

HEADERS = {
    "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
    "Accept-Language": "en-US,en;q=0.9",
    "Referer": f"{BASE_URL}/",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
}


def _session() -> requests.Session:
    """创建带全局配置的 requests.Session"""
    config = get_config()
    sess = requests.Session()
    sess.headers.update(HEADERS)
    if config.proxy:
        sess.proxies = {"http": config.proxy, "https": config.proxy}
    sess.verify = not config.insecure
    if config.headers:
        sess.headers.update(config.headers)
    return sess


def _cookie_header(sess: requests.Session) -> str:
    """将 session cookies 序列化为 Cookie 请求头字符串"""
    return "; ".join(f"{k}={v}" for k, v in sess.cookies.items())


def _extract_email(html: str) -> str:
    """从 HTML 中提取邮箱地址"""
    match = EMAIL_RE.search(html)
    if not match:
        match = EMAIL_RE_ALT.search(html)
    if not match:
        raise RuntimeError("lroid: 无法从 HTML 响应中解析邮箱地址")
    addr = match.group(1).strip()
    if "@" not in addr:
        raise RuntimeError("lroid: 解析到的邮箱地址无效")
    return addr


def _decode_token(token: str) -> str:
    """从 JSON token 中提取 cookie 字符串"""
    try:
        data = json.loads(token)
    except Exception as exc:
        raise ValueError("lroid: 无效的 session token") from exc
    cookie = str(data.get("cookie") or "").strip()
    if not cookie:
        raise ValueError("lroid: session token 中缺少 cookie")
    return cookie


def generate_email(channel: str = CHANNEL) -> EmailInfo:
    """
    创建临时邮箱
    GET 首页，服务端自动分配邮箱地址并通过 session cookies 绑定
    从 HTML 中的 <input id="eposta_adres" value="..."> 提取邮箱地址
    """
    sess = _session()
    config = get_config()

    resp = sess.get(BASE_URL, timeout=config.timeout)
    resp.raise_for_status()

    email = _extract_email(resp.text)

    # 将 session cookies 序列化为 JSON token，获取邮件时需要
    token = json.dumps({"cookie": _cookie_header(sess)})
    return EmailInfo(channel=channel, email=email, _token=token)


def get_emails(token: str, email: str = "") -> List[Email]:
    """
    获取邮件列表
    尝试使用 kontrol API（GET /en/api-kontrol/）获取 JSON 格式邮件
    若 API 不可用则回退到 HTML 页面解析
    """
    cookie = _decode_token(token)
    address = (email or "").strip()
    if not address:
        raise ValueError("lroid: 邮箱地址不能为空")

    from .. import http as tm_http

    # 尝试 kontrol API
    try:
        resp = tm_http.get(
            f"{BASE_URL}/en/api-kontrol/",
            headers={
                "Accept": "application/json, text/html, */*;q=0.8",
                "Referer": f"{BASE_URL}/",
                "Cookie": cookie,
                "User-Agent": HEADERS["User-Agent"],
            },
            timeout=15,
        )
        resp.raise_for_status()
        data = resp.json()
        if isinstance(data, list):
            return _parse_json_emails(data, address)
        if isinstance(data, dict):
            # 响应可能将邮件列表放在某个字段中
            for key in ("mails", "emails", "messages", "data", "inbox"):
                items = data.get(key)
                if isinstance(items, list):
                    return _parse_json_emails(items, address)
            # 整个 dict 可能就是单封邮件
            if data.get("id") or data.get("subject") or data.get("from"):
                return _parse_json_emails([data], address)
    except Exception:
        pass

    # API 不可用，回退到 HTML 页面解析
    return _parse_html_emails(cookie, address, tm_http)


def _parse_json_emails(items: list, recipient: str) -> List[Email]:
    """解析 JSON 格式的邮件列表"""
    emails: List[Email] = []
    for raw in items:
        if not isinstance(raw, dict):
            continue
        normalized = dict(raw)
        # 将常见的非标准字段映射为 normalize 可识别的字段
        if "body" in normalized and "html" not in normalized:
            normalized["html"] = normalized["body"]
        if "content" in normalized and "html" not in normalized:
            normalized["html"] = normalized["content"]
        emails.append(normalize_email(normalized, recipient))
    return emails


def _parse_html_emails(cookie: str, recipient: str, tm_http) -> List[Email]:
    """从 HTML 页面解析邮件列表"""
    resp = tm_http.get(
        BASE_URL,
        headers={
            "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            "Referer": f"{BASE_URL}/",
            "Cookie": cookie,
            "User-Agent": HEADERS["User-Agent"],
        },
        timeout=15,
    )
    resp.raise_for_status()
    html = resp.text

    emails: List[Email] = []

    # 解析 <li class="mail"> 元素中的邮件摘要信息
    mail_items = re.findall(
        r'<li[^>]*class=["\'][^"\']*\bmail\b[^"\']*["\'][^>]*>(.*?)</li>',
        html,
        re.DOTALL | re.IGNORECASE,
    )

    for idx, item_html in enumerate(mail_items):
        mail_id = ""
        subject = ""
        from_addr = ""
        date = ""

        # 提取邮件链接中的 ID
        link_match = re.search(r'href=["\']/?([^"\']*?/?\d+)["\']', item_html, re.I)
        if link_match:
            mail_id = link_match.group(1).strip().rstrip("/").rsplit("/", 1)[-1]

        # 提取 data-id 属性
        if not mail_id:
            data_id_match = re.search(r'data-id=["\']([^"\']+)["\']', item_html, re.I)
            if data_id_match:
                mail_id = data_id_match.group(1).strip()

        if not mail_id:
            mail_id = str(idx + 1)

        # 提取主题
        subject_match = re.search(r'class=["\'][^"\']*\bsubject\b[^"\']*["\'][^>]*>(.*?)</', item_html, re.DOTALL | re.I)
        if subject_match:
            subject = re.sub(r"<[^>]+>", "", subject_match.group(1)).strip()

        # 提取发件人
        from_match = re.search(r'class=["\'][^"\']*\b(?:from|sender)\b[^"\']*["\'][^>]*>(.*?)</', item_html, re.DOTALL | re.I)
        if from_match:
            from_addr = re.sub(r"<[^>]+>", "", from_match.group(1)).strip()

        # 提取日期
        date_match = re.search(r'class=["\'][^"\']*\b(?:date|time)\b[^"\']*["\'][^>]*>(.*?)</', item_html, re.DOTALL | re.I)
        if date_match:
            date = re.sub(r"<[^>]+>", "", date_match.group(1)).strip()

        # 尝试获取邮件详情
        body_html = ""
        body_text = ""
        if mail_id and mail_id.isdigit():
            try:
                detail = _fetch_mail_detail(cookie, mail_id, tm_http)
                body_html = detail.get("html", "")
                body_text = detail.get("text", "")
                if not subject and detail.get("subject"):
                    subject = detail["subject"]
                if not from_addr and detail.get("from"):
                    from_addr = detail["from"]
                if not date and detail.get("date"):
                    date = detail["date"]
            except Exception:
                pass

        emails.append(Email(
            id=mail_id,
            from_addr=from_addr,
            to=recipient,
            subject=subject,
            text=body_text,
            html=body_html,
            date=date,
            is_read=False,
            attachments=[],
        ))

    return emails


def _fetch_mail_detail(cookie: str, mail_id: str, tm_http) -> dict:
    """获取单封邮件的详细内容"""
    resp = tm_http.get(
        f"{BASE_URL}/{mail_id}",
        headers={
            "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            "Referer": f"{BASE_URL}/",
            "Cookie": cookie,
            "User-Agent": HEADERS["User-Agent"],
        },
        timeout=15,
    )
    resp.raise_for_status()
    html = resp.text

    result = {"html": "", "text": "", "subject": "", "from": "", "date": ""}

    # 提取 iframe 内容（邮件正文通常放在 iframe 中）
    iframe_match = re.search(r'<iframe[^>]+srcdoc=["\']([^"\']+)["\']', html, re.DOTALL | re.I)
    if iframe_match:
        import html as html_mod
        result["html"] = html_mod.unescape(iframe_match.group(1))
    else:
        # 尝试提取 iframe src
        iframe_src_match = re.search(r'<iframe[^>]+src=["\']([^"\']+)["\']', html, re.DOTALL | re.I)
        if iframe_src_match:
            src = iframe_src_match.group(1)
            if not src.startswith("http"):
                src = f"{BASE_URL}/{src.lstrip('/')}"
            try:
                iframe_resp = tm_http.get(
                    src,
                    headers={
                        "Accept": "text/html, */*",
                        "Referer": f"{BASE_URL}/{mail_id}",
                        "Cookie": cookie,
                        "User-Agent": HEADERS["User-Agent"],
                    },
                    timeout=15,
                )
                iframe_resp.raise_for_status()
                result["html"] = iframe_resp.text
            except Exception:
                pass

    # 如果没有从 iframe 提取到内容，尝试从页面主体提取
    if not result["html"]:
        body_match = re.search(
            r'class=["\'][^"\']*\b(?:mail-body|mail-content|message-body|content)\b[^"\']*["\'][^>]*>(.*?)</div>',
            html,
            re.DOTALL | re.IGNORECASE,
        )
        if body_match:
            result["html"] = body_match.group(1).strip()

    # 从 HTML 提取纯文本
    if result["html"]:
        result["text"] = re.sub(r"<[^>]+>", "", result["html"]).strip()

    # 提取主题
    subj_match = re.search(r'class=["\'][^"\']*\bsubject\b[^"\']*["\'][^>]*>(.*?)</', html, re.DOTALL | re.I)
    if subj_match:
        result["subject"] = re.sub(r"<[^>]+>", "", subj_match.group(1)).strip()

    # 提取发件人
    from_match = re.search(r'class=["\'][^"\']*\b(?:from|sender)\b[^"\']*["\'][^>]*>(.*?)</', html, re.DOTALL | re.I)
    if from_match:
        result["from"] = re.sub(r"<[^>]+>", "", from_match.group(1)).strip()

    # 提取日期
    date_match = re.search(r'class=["\'][^"\']*\b(?:date|time)\b[^"\']*["\'][^>]*>(.*?)</', html, re.DOTALL | re.I)
    if date_match:
        result["date"] = re.sub(r"<[^>]+>", "", date_match.group(1)).strip()

    return result
