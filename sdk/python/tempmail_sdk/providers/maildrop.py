"""
Maildrop — https://maildrop.cx
GET /api/suffixes.php、GET /api/emails.php
"""

import random
import string
from urllib.parse import urlencode

from .. import http as tm_http
from ..types import EmailInfo, Email

CHANNEL = "maildrop"
BASE = "https://maildrop.cx"
EXCLUDED_SUFFIX = "transformer.edu.kg"

_HEADERS = {
    "Accept": "application/json, text/plain, */*",
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "Cache-Control": "no-cache",
    "DNT": "1",
    "Pragma": "no-cache",
    "Referer": "https://maildrop.cx/zh-cn/app",
    "sec-ch-ua": '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
    "sec-ch-ua-mobile": "?0",
    "sec-ch-ua-platform": '"Windows"',
    "sec-fetch-dest": "empty",
    "sec-fetch-mode": "cors",
    "sec-fetch-site": "same-origin",
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    "x-requested-with": "XMLHttpRequest",
}


def _random_local(length: int = 10) -> str:
    return "".join(random.choices(string.ascii_lowercase + string.digits, k=length))


def _fetch_suffixes() -> list:
    r = tm_http.get(f"{BASE}/api/suffixes.php", headers=_HEADERS)
    r.raise_for_status()
    data = r.json()
    if not isinstance(data, list):
        raise ValueError("maildrop: invalid suffixes response")
    ex = EXCLUDED_SUFFIX.lower()
    out = []
    for x in data:
        if isinstance(x, str):
            t = x.strip()
            if t and t.lower() != ex:
                out.append(t)
    if not out:
        raise ValueError("maildrop: no domains available")
    return out


def _pick_suffix(suffixes: list, preferred: str | None) -> str:
    if preferred and str(preferred).strip():
        p = str(preferred).strip().lower()
        for d in suffixes:
            if d.lower() == p:
                return d
        raise ValueError(f"maildrop: domain not available: {p}")
    return random.choice(suffixes)


def _cx_date_to_iso(s: str) -> str:
    s = (s or "").strip()
    if len(s) == 19 and s[10] == " ":
        return s[:10] + "T" + s[11:] + "Z"
    return s


def generate_email(domain: str | None = None, **kwargs) -> EmailInfo:
    """随机本地部分；域名来自 suffixes，排除 transformer.edu.kg。"""
    suffixes = _fetch_suffixes()
    dom = _pick_suffix(suffixes, domain)
    local = _random_local()
    email = f"{local}@{dom}"
    return EmailInfo(channel=CHANNEL, email=email, _token=email)


import json as _json


def _fetch_detail(mail_id: str) -> dict | None:
    """
    通过详情接口获取单封邮件完整内容
    GET /api/email_content.php?id={id}
    详情响应字段（从前端代码确认）:
      - content: 完整 HTML 正文
      - subject / from_addr / date: 邮件元数据
      - attachment: JSON 字符串数组 [{filename, path, size}]（可能为空）
    失败时返回 None，调用方回退到列表 description
    """
    mid = (mail_id or "").strip()
    if not mid:
        return None
    try:
        r = tm_http.get(
            f"{BASE}/api/email_content.php",
            headers=_HEADERS,
            params={"id": mid},
            timeout=15,
        )
        if r.status_code < 200 or r.status_code >= 300:
            return None
        data = r.json() if r.content else None
        if isinstance(data, dict):
            return data
    except Exception:  # noqa: BLE001
        return None
    return None


def _parse_attachments(raw) -> list:
    """解析详情接口的 attachment 字段（JSON 字符串）为附件字典列表"""
    if not raw or not isinstance(raw, str) or not raw.strip():
        return []
    try:
        items = _json.loads(raw)
    except (ValueError, TypeError):
        return []
    if not isinstance(items, list):
        return []
    out = []
    for it in items:
        if not isinstance(it, dict):
            continue
        filename = it.get("filename")
        if not isinstance(filename, str) or not filename.strip():
            continue
        entry = {"filename": filename.strip()}
        sz = it.get("size")
        if isinstance(sz, (int, float)) and not isinstance(sz, bool):
            entry["size"] = int(sz)
        out.append(entry)
    return out


def get_emails(token: str, email: str = "", **kwargs) -> list:
    """
    获取邮件列表并对每封邮件补拉详情
    1. GET /api/emails.php?addr={email} 拉取列表（仅含 description 摘要）
    2. 对每封邮件 GET /api/email_content.php?id={id} 拉取详情（含 content 完整 HTML）
    3. 详情失败时保留列表 description 作为回退
    """
    addr = (email or "").strip() or (token or "").strip()
    if not addr:
        raise ValueError("maildrop: empty address")
    qs = urlencode({"addr": addr, "page": "1", "limit": "20"})
    r = tm_http.get(f"{BASE}/api/emails.php?{qs}", headers=_HEADERS)
    r.raise_for_status()
    data = r.json() if r.content else {}
    rows = data.get("emails") or []
    if not isinstance(rows, list):
        return []
    out = []
    for row in rows:
        if not isinstance(row, dict):
            continue
        mail_id = str(row.get("id", ""))
        desc = (row.get("description") or "").strip()
        ir = row.get("isRead")
        is_read = ir is True or ir == 1

        from_addr = (row.get("from_addr") or "").strip()
        subject = (row.get("subject") or "").strip()
        date = _cx_date_to_iso(row.get("date") or "")
        html_body = ""
        attachments: list = []

        # 拉取详情覆盖 html/from/subject/date/attachments
        if mail_id:
            detail = _fetch_detail(mail_id)
            if detail:
                content = detail.get("content")
                if isinstance(content, str) and content.strip():
                    html_body = content
                d_from = detail.get("from_addr")
                if isinstance(d_from, str) and d_from.strip():
                    from_addr = d_from.strip()
                d_subj = detail.get("subject")
                if isinstance(d_subj, str) and d_subj.strip():
                    subject = d_subj.strip()
                d_date = detail.get("date")
                if isinstance(d_date, str) and d_date.strip():
                    date = _cx_date_to_iso(d_date)
                attachments = _parse_attachments(detail.get("attachment"))

        out.append(
            Email(
                id=mail_id,
                from_addr=from_addr,
                to=addr,
                subject=subject,
                text=desc,
                html=html_body,
                date=date,
                is_read=is_read,
                attachments=attachments,
            )
        )
    return out
