"""
10minutemail.net 临时邮箱渠道

流程：GET / 获取 session cookie（PHPSESSID）+ 从 HTML 提取随机分配的邮箱地址
      GET /mailbox.ajax.php?_={毫秒时间戳} 获取邮件列表（HTML 表格，非 JSON）
      GET /readmail.html?mid={id} 获取单封邮件整页 HTML，从中提取正文片段
token 序列化保存 session cookie 串，供 get_emails 复用（参考 moakt 模式）。
"""

import base64
import json
import re
import time
from typing import Dict, List

import requests

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "10minutemail-net"
BASE_URL = "https://10minutemail.net"
_TOK_PREFIX = "tmn1:"

# 从首页 HTML 的邮箱输入框提取地址（value="xxx@xxx.com"）
_EMAIL_RE = re.compile(r'value="([^"]+@[^"]+)"')
# 逐行匹配 <tr>...</tr>
_ROW_RE = re.compile(r"(?is)<tr[^>]*>(.*?)</tr>")
# 从行内链接提取邮件 ID（readmail.html?mid=xxx）
_MID_RE = re.compile(r"(?i)readmail\.html\?mid=([^'\"&]+)")
# 逐个匹配行内 <td>...</td> 单元格
_CELL_RE = re.compile(r"(?is)<td[^>]*>(.*?)</td>")
# 从单元格提取 span 的 title 属性（收件时间）
_TITLE_RE = re.compile(r'(?i)title="([^"]+)"')
# 提取 Cloudflare 邮箱保护混淆数据（data-cfemail="十六进制"）
_CF_RE = re.compile(r'(?i)data-cfemail="([0-9a-fA-F]+)"')
# 去除 HTML 标签
_TAG_RE = re.compile(r"(?s)<[^>]+>")

_UA = (
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
)


def _browser_headers() -> Dict[str, str]:
    return {
        "User-Agent": _UA,
        "Accept": (
            "text/html,application/xhtml+xml,application/xml;q=0.9,"
            "image/avif,image/webp,image/apng,*/*;q=0.8"
        ),
        "Accept-Language": "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
    }


def _ajax_headers() -> Dict[str, str]:
    return {
        "User-Agent": _UA,
        "Accept": "*/*",
        "Accept-Language": "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
        "X-Requested-With": "XMLHttpRequest",
        "Referer": f"{BASE_URL}/",
    }


def _cookie_header(resp: requests.Response) -> str:
    d = resp.cookies.get_dict()
    return "; ".join(f"{k}={d[k]}" for k in sorted(d.keys()))


def _encode_token(cookie_hdr: str) -> str:
    raw = json.dumps({"c": cookie_hdr}, separators=(",", ":")).encode("utf-8")
    return _TOK_PREFIX + base64.b64encode(raw).decode("ascii")


def _decode_token(token: str) -> str:
    if not token.startswith(_TOK_PREFIX):
        # 兼容空/异常 token：无 cookie 时仍尝试请求（依赖底层 session jar）
        return ""
    try:
        data = base64.b64decode(token[len(_TOK_PREFIX):])
        o = json.loads(data.decode("utf-8"))
    except (ValueError, UnicodeDecodeError):
        return ""
    return str(o.get("c") or "").strip()


def generate_email(channel: str = CHANNEL) -> EmailInfo:
    """
    创建 10minutemail.net 临时邮箱

    1. GET / 获取 session cookie（PHPSESSID）
    2. 从首页 HTML 的输入框正则提取随机分配的邮箱地址
    token 序列化保存 session cookie，供后续 mailbox.ajax.php 复用。
    """
    resp = tm_http.get(f"{BASE_URL}/", headers=_browser_headers())
    resp.raise_for_status()

    cookie_hdr = _cookie_header(resp)

    m = _EMAIL_RE.search(resp.text)
    if not m:
        raise RuntimeError("10minutemail-net: 未能从首页提取邮箱地址")
    email = m.group(1).strip()
    if not email or "@" not in email:
        raise RuntimeError(f"10minutemail-net: 获取到的邮箱地址无效: {email!r}")

    return EmailInfo(channel=channel, email=email, _token=_encode_token(cookie_hdr))


def get_emails(email: str, token: str) -> List[Email]:
    """
    获取 10minutemail.net 邮件列表

    1. GET /mailbox.ajax.php?_={毫秒时间戳} 获取邮件列表（HTML 表格）
    2. 逐行解析 <tr>，提取邮件 ID、发件人、主题、收件时间、已读状态
    3. 对每封邮件 GET /readmail.html?mid={id} 获取正文 HTML 片段
    表格列顺序: 寄件人 | 主题 | 收件日期；未读行的 <tr> 带 font-weight: bold 样式。
    """
    email = (email or "").strip()
    if not email:
        raise ValueError("10minutemail-net: 邮箱地址为空")
    cookie_hdr = _decode_token((token or "").strip())

    headers = _ajax_headers()
    if cookie_hdr:
        headers["Cookie"] = cookie_hdr
    list_url = f"{BASE_URL}/mailbox.ajax.php?_={int(time.time() * 1000)}"
    resp = tm_http.get(list_url, headers=headers)
    resp.raise_for_status()

    out: List[Email] = []
    for row_full, row_inner in _ROW_RE.findall(resp.text):
        # 跳过表头行（含 <th>）
        if "<th" in row_inner.lower():
            continue

        mid_match = _MID_RE.search(row_inner)
        if not mid_match:
            continue
        mail_id = mid_match.group(1).strip()
        if not mail_id:
            continue

        cells = _CELL_RE.findall(row_inner)
        from_cell = cells[0] if len(cells) > 0 else ""
        subject_cell = cells[1] if len(cells) > 1 else ""
        date_cell = cells[2] if len(cells) > 2 else ""

        from_addr = _extract_text(from_cell)
        subject = _extract_text(subject_cell)

        # 收件时间优先取 span 的 title 属性（UTC 绝对时间），否则取单元格文本
        tm = _TITLE_RE.search(date_cell)
        date = tm.group(1).strip() if tm else _extract_text(date_cell)

        # 未读状态：行 <tr> 样式含 font-weight: bold
        is_read = "font-weight: bold" not in row_full.lower()

        raw = {
            "id": mail_id,
            "from": from_addr,
            "to": email,
            "subject": subject,
            "html": _fetch_body(cookie_hdr, mail_id),
            "date": date,
            "isRead": is_read,
        }
        out.append(normalize_email(raw, email))

    return out


def _fetch_body(cookie_hdr: str, mail_id: str) -> str:
    """
    获取单封邮件的正文 HTML 片段（GET /readmail.html?mid={id}）。
    正文位于 <div class="mailinhtml">...</div>，失败或未找到时返回空串。
    """
    if not mail_id:
        return ""
    headers = _browser_headers()
    headers["Referer"] = f"{BASE_URL}/"
    if cookie_hdr:
        headers["Cookie"] = cookie_hdr
    try:
        resp = tm_http.get(f"{BASE_URL}/readmail.html?mid={mail_id}", headers=headers)
    except OSError:
        return ""
    if resp.status_code < 200 or resp.status_code >= 300:
        return ""
    return _extract_body(resp.text)


def _extract_body(page: str) -> str:
    """
    从整页 HTML 提取正文片段。
    正文包裹于 <div class="mailinhtml"> ... </div>，内部存在嵌套 div，
    以紧随其后的 email-decode.min.js script 标签作为结束锚点，
    再回退裁掉尾部包裹的两个 </div>。
    """
    start_mark = 'class="mailinhtml"'
    si = page.find(start_mark)
    if si < 0:
        return ""
    gt = page.find(">", si)
    if gt < 0:
        return ""
    rest = page[gt + 1:]

    ei = rest.find("email-decode.min.js")
    if ei < 0:
        # 兜底：退化为该 div 后第一个 </div>
        di = rest.find("</div>")
        if di < 0:
            return rest.strip()
        return rest[:di].strip()

    segment = rest[:ei]
    s_idx = segment.rfind("<script")
    if s_idx >= 0:
        segment = segment[:s_idx]
    segment = segment.strip()
    # 去掉 mailinhtml 与其外层 tab1 的两个闭合 div
    for _ in range(2):
        segment = segment.strip()
        if segment.endswith("</div>"):
            segment = segment[: -len("</div>")]
    return segment.strip()


def _extract_text(cell: str) -> str:
    """
    从单元格 HTML 提取纯文本发件人/主题。
    优先解码 Cloudflare 邮箱保护混淆（__cf_email__ + data-cfemail），否则去标签取文本。
    """
    cf = _CF_RE.search(cell)
    if cf:
        decoded = _cf_decode(cf.group(1))
        if decoded:
            return decoded
    text = _TAG_RE.sub("", cell)
    for ent, ch in (
        ("&nbsp;", " "),
        ("&#160;", " "),
        ("&amp;", "&"),
        ("&lt;", "<"),
        ("&gt;", ">"),
        ("&quot;", '"'),
    ):
        text = text.replace(ent, ch)
    return text.strip()


def _cf_decode(encoded: str) -> str:
    """
    解码 Cloudflare 邮箱保护混淆字符串。
    算法：首字节为异或密钥，其后每字节与密钥异或还原 ASCII。失败返回空串。
    """
    try:
        data = bytes.fromhex(encoded)
    except ValueError:
        return ""
    if len(data) < 2:
        return ""
    key = data[0]
    decoded = bytes(b ^ key for b in data[1:]).decode("utf-8", "replace")
    if "@" not in decoded:
        return ""
    return decoded
