"""
tempemail.co 渠道 — https://tempemail.co
Cookie Session + REST JSON API。
创建邮箱: GET /mail/random 获取随机邮箱地址，自动管理 session cookie。
获取邮件: GET /get-mails 获取邮件列表（HTML 包裹在 JSON 中），GET /mail/info 获取邮件详情。
域名: @fmail10.de（由 API 返回，可能变化）
"""

import json
import re
from typing import Dict, List

import requests

from ..config import get_config
from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "tempemail-co"
BASE_URL = "https://tempemail.co"

# 从邮件列表 HTML 中提取邮件 ID 的正则（data-id="数字"）
_MAIL_ID_RE = re.compile(r'data-id="(\d+)"')

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


def _encode_token(address: str, cookies: Dict[str, str]) -> str:
    """将邮箱地址和 cookie 字典编码为 token 字符串（JSON）"""
    return json.dumps({"address": address, "cookies": cookies}, separators=(",", ":"))


def _decode_token(token: str) -> tuple:
    """从 token 字符串解码出邮箱地址和 cookie 字典，返回 (address, cookies_dict)"""
    try:
        data = json.loads(token)
    except (json.JSONDecodeError, ValueError) as e:
        raise ValueError("tempemail-co: token 格式无效") from e
    address = data.get("address", "")
    cookies = data.get("cookies", {})
    if not address or not isinstance(cookies, dict):
        raise ValueError("tempemail-co: token 数据不完整")
    return address, cookies


def generate_email() -> EmailInfo:
    """
    创建 tempemail.co 临时邮箱

    流程:
    1. GET /mail/random 获取随机邮箱地址，保存 session cookie
    2. 返回 {"result":true,"id":"user@fmail10.de","address":"user@fmail10.de"}
    3. token 存储 JSON: {"address": "user@fmail10.de", "cookies": {...}}
    """
    r = tm_http.get(
        f"{BASE_URL}/mail/random",
        headers=_build_headers(
            referer=f"{BASE_URL}/",
            accept="application/json, text/javascript, */*; q=0.01",
        ),
    )
    r.raise_for_status()

    data = r.json()
    if not data.get("result"):
        raise RuntimeError(f"tempemail-co: 创建邮箱失败: {data!r}")

    address = (data.get("address") or data.get("id") or "").strip()
    if not address or "@" not in address:
        raise RuntimeError(f"tempemail-co: 返回的邮箱地址无效: {data!r}")

    # 收集 session cookie
    cookies = r.cookies.get_dict()

    tok = _encode_token(address, cookies)
    return EmailInfo(channel=CHANNEL, email=address, _token=tok)


def get_emails(email: str, token: str) -> List[Email]:
    """
    获取 tempemail.co 邮件列表

    流程:
    1. 从 token 解码出邮箱地址和 cookie
    2. GET /get-mails?mail_id={address}&unseen=0&is_new=1 获取邮件列表
       - 返回 JSON: {"result":true,"mails":"<html>","count":0}
       - count == 0 时无邮件
    3. 当 count > 0 时，从 mails HTML 中正则提取邮件 ID（data-id="数字"）
    4. 对每个邮件 ID，GET /mail/info?id={id} 获取邮件详情
       - 返回 JSON: {"result":true,"mail":{"fromName":"","fromAddress":"xxx","displayDate":"xxx","subject":"xxx","textHtml":"<html>"}}
    5. 使用 normalize_email 标准化返回结果
    """
    if not token:
        raise ValueError("tempemail-co: token 不能为空")

    addr = (email or "").strip()
    if not addr:
        raise ValueError("tempemail-co: 邮箱地址不能为空")

    address, cookies = _decode_token(token)
    cookie_str = _cookies_to_str(cookies)

    # 获取邮件列表
    r = tm_http.get(
        f"{BASE_URL}/get-mails",
        headers=_build_headers(
            referer=f"{BASE_URL}/",
            cookie=cookie_str,
            accept="application/json, text/javascript, */*; q=0.01",
        ),
        params={"mail_id": address, "unseen": "0", "is_new": "1"},
    )
    r.raise_for_status()

    data = r.json()
    count = data.get("count", 0)
    if not count or count <= 0:
        return []

    # 从 mails HTML 中提取邮件 ID
    mails_html = data.get("mails", "")
    if not mails_html or not isinstance(mails_html, str):
        return []

    mail_ids = _MAIL_ID_RE.findall(mails_html)
    if not mail_ids:
        return []

    # 去重并保持顺序
    seen = set()
    unique_ids = []
    for mid in mail_ids:
        if mid not in seen:
            seen.add(mid)
            unique_ids.append(mid)

    out: List[Email] = []
    for mail_id in unique_ids:
        # 获取邮件详情
        try:
            detail_resp = tm_http.get(
                f"{BASE_URL}/mail/info",
                headers=_build_headers(
                    referer=f"{BASE_URL}/",
                    cookie=cookie_str,
                    accept="application/json, text/javascript, */*; q=0.01",
                ),
                params={"id": mail_id},
            )
            if detail_resp.status_code != 200:
                continue

            detail_data = detail_resp.json()
            if not detail_data.get("result"):
                continue

            mail_info = detail_data.get("mail", {})
            if not isinstance(mail_info, dict):
                continue

            # 构造发件人显示字符串
            from_name = (mail_info.get("fromName") or "").strip()
            from_addr = (mail_info.get("fromAddress") or "").strip()
            from_display = f"{from_name} <{from_addr}>" if from_name else from_addr

            raw_email = {
                "id": str(mail_id),
                "from": from_display,
                "from_address": from_addr,
                "from_name": from_name,
                "to": addr,
                "subject": mail_info.get("subject", ""),
                "html": mail_info.get("textHtml", ""),
                "date": mail_info.get("displayDate", ""),
            }
            out.append(normalize_email(raw_email, addr))
        except Exception:
            # 单封邮件获取失败不影响其他邮件
            continue

    return out
