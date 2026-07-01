"""
Rootsh(BccTo) 渠道 — https://rootsh.com
需要 cookie session 认证，token 存储 time 值用于增量获取邮件
创建邮箱：GET / 获取 session cookie，POST /applymail 申请邮箱
获取邮件：POST /getmail 获取邮件列表，POST /viewmail 获取邮件正文
"""

import json
import random
import string
import time
from typing import List, Optional

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "rootsh"
BASE = "https://rootsh.com"

# 默认域名
DEFAULT_DOMAIN = "bccto.cc"

_HEADERS = {
    "Accept": "application/json, text/plain, */*",
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "Cache-Control": "no-cache",
    "DNT": "1",
    "Pragma": "no-cache",
    "Referer": f"{BASE}/",
    "X-Requested-With": "XMLHttpRequest",
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

# token 前缀，用于区分序列化格式
_TOK_PREFIX = "rootsh1:"


def _random_local(length: int = 10) -> str:
    """生成随机邮箱本地部分"""
    return "".join(random.choices(string.ascii_lowercase + string.digits, k=length))


def _encode_token(last_check_time: int, cookies: dict) -> str:
    """将 lastCheckTime 和 cookies 编码为 token 字符串"""
    payload = json.dumps({"t": last_check_time, "c": cookies}, separators=(",", ":"))
    return _TOK_PREFIX + payload


def _decode_token(token: str) -> tuple:
    """从 token 中解码出 lastCheckTime 和 cookies"""
    if not token or not token.startswith(_TOK_PREFIX):
        raise ValueError("rootsh: 无效的 session token")
    try:
        data = json.loads(token[len(_TOK_PREFIX):])
    except (json.JSONDecodeError, ValueError) as e:
        raise ValueError("rootsh: 无效的 session token") from e
    last_check_time = data.get("t", 0)
    cookies = data.get("c", {})
    if not isinstance(cookies, dict):
        raise ValueError("rootsh: 无效的 session token")
    return last_check_time, cookies


def _build_cookie_header(cookies: dict) -> str:
    """将 cookies 字典构建为 Cookie 请求头字符串"""
    return "; ".join(f"{k}={v}" for k, v in cookies.items())


def _merge_cookies(existing: dict, response) -> dict:
    """合并响应中的 Set-Cookie 到现有 cookies"""
    merged = dict(existing)
    merged.update(response.cookies.get_dict())
    return merged


def generate_email(domain: Optional[str] = None, channel: str = CHANNEL) -> EmailInfo:
    """
    创建 rootsh 临时邮箱
    1. GET / 获取 session cookie
    2. POST /applymail 申请邮箱地址
    """
    dom = (domain or "").strip() or DEFAULT_DOMAIN
    local = _random_local()
    mail_addr = f"{local}@{dom}"

    # 第一步：GET 首页获取 session cookie
    r1 = tm_http.get(f"{BASE}/", headers={
        "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
        "User-Agent": _HEADERS["User-Agent"],
    })
    r1.raise_for_status()
    cookies = r1.cookies.get_dict()

    # 第二步：POST /applymail 申请邮箱
    r2 = tm_http.post(
        f"{BASE}/applymail",
        headers={
            **_HEADERS,
            "Content-Type": "application/x-www-form-urlencoded; charset=UTF-8",
            "Cookie": _build_cookie_header(cookies),
        },
        data=f"mail={mail_addr}",
    )
    r2.raise_for_status()
    cookies = _merge_cookies(cookies, r2)

    body = r2.json()
    if not isinstance(body, dict):
        raise RuntimeError("rootsh: applymail 响应格式无效")

    success = body.get("success")
    if str(success) != "true":
        tips = body.get("tips", "")
        raise RuntimeError(f"rootsh: 邮箱申请失败: {tips}")

    # 使用服务端返回的邮箱地址（可能与请求的不同）
    actual_email = (body.get("user") or "").strip() or mail_addr
    # time 值用于后续增量获取邮件
    last_check_time = body.get("time", 0)

    token = _encode_token(last_check_time, cookies)
    return EmailInfo(channel=channel, email=actual_email, _token=token)


def get_emails(token: str, email: str = "") -> List[Email]:
    """
    获取 rootsh 邮件列表
    1. POST /getmail 获取邮件摘要列表
    2. POST /viewmail 获取每封邮件的 HTML 正文
    """
    if not token:
        raise ValueError("rootsh: token 不能为空")
    if not (email or "").strip():
        raise ValueError("rootsh: 邮箱地址不能为空")

    addr = email.strip()
    last_check_time, cookies = _decode_token(token)

    # 当前毫秒时间戳
    ts = int(time.time() * 1000)

    # POST /getmail 获取邮件列表
    r = tm_http.post(
        f"{BASE}/getmail",
        headers={
            **_HEADERS,
            "Content-Type": "application/x-www-form-urlencoded; charset=UTF-8",
            "Cookie": _build_cookie_header(cookies),
        },
        data=f"mail={addr}&time={last_check_time}&_={ts}",
    )
    r.raise_for_status()
    body = r.json()

    if not isinstance(body, dict):
        return []

    if str(body.get("success")) != "true":
        return []

    mail_list = body.get("mail", [])
    if not isinstance(mail_list, list):
        return []

    out: List[Email] = []
    for item in mail_list:
        # 每个 item 是数组: [display_name, from_email, subject, date_str, mail_fid, received_time]
        if not isinstance(item, list) or len(item) < 5:
            continue

        display_name = str(item[0]) if item[0] else ""
        from_email = str(item[1]) if item[1] else ""
        subject = str(item[2]) if item[2] else ""
        date_str = str(item[3]) if item[3] else ""
        mail_fid = str(item[4]) if item[4] else ""

        # 优先使用 from_email，如果为空则使用 display_name
        from_addr = from_email or display_name

        # POST /viewmail 获取邮件正文
        html_content = ""
        if mail_fid:
            try:
                rv = tm_http.post(
                    f"{BASE}/viewmail",
                    headers={
                        **_HEADERS,
                        "Content-Type": "application/x-www-form-urlencoded; charset=UTF-8",
                        "Cookie": _build_cookie_header(cookies),
                    },
                    data=f"mail={mail_fid}&to={addr}",
                )
                if rv.status_code == 200:
                    vb = rv.json()
                    if isinstance(vb, dict) and str(vb.get("success")) == "true":
                        html_content = vb.get("mail", "")
            except Exception:
                pass

        raw = {
            "id": mail_fid,
            "from": from_addr,
            "to": addr,
            "subject": subject,
            "date": date_str,
            "html": html_content,
        }
        out.append(normalize_email(raw, addr))

    return out
