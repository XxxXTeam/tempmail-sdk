"""
smailpro.com 临时邮箱渠道

两段式流程：
  1. GET https://smailpro.com/app/payload?url={目标API}[&email=&mid=] → 返回 JWT（纯文本）
  2. 携带 JWT 调用 api.sonjj.com 对应接口（payload={JWT}）
创建邮箱、获取列表、获取详情均需先取 payload 再调用 sonjj API。
本渠道无需持久化认证信息，get_emails 仅依赖邮箱地址；token 仅为占位以满足上层校验。
"""

from typing import Dict, List, Optional, Tuple

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "smailpro"
BASE_URL = "https://smailpro.com"
API_BASE_URL = "https://api.sonjj.com/v1/temp_email"

_HEADERS = {
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
    "Accept": "application/json, text/plain, */*",
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
    "Referer": f"{BASE_URL}/",
}


def _fetch_payload(target_url: str, extra: Optional[Dict[str, str]] = None) -> str:
    """获取访问 sonjj API 所需的 JWT；extra 为附加查询参数（email、mid）"""
    params: Dict[str, str] = {"url": target_url}
    if extra:
        params.update(extra)

    resp = tm_http.get(f"{BASE_URL}/app/payload", headers=_HEADERS, params=params)
    resp.raise_for_status()

    # payload 接口返回纯文本 JWT，去除可能的引号与空白
    payload = resp.text.strip().strip('"')
    if not payload:
        raise RuntimeError("smailpro: payload 为空")
    return payload


def _call_api(target_url: str, extra: Optional[Dict[str, str]], label: str) -> dict:
    """携带 JWT 调用 sonjj API 并返回解析后的 JSON"""
    payload = _fetch_payload(target_url, extra)
    resp = tm_http.get(target_url, headers=_HEADERS, params={"payload": payload})
    resp.raise_for_status()
    return resp.json()


def generate_email(channel: str = CHANNEL) -> EmailInfo:
    """
    创建 smailpro 临时邮箱

    调用 sonjj create 接口，返回 {"email":"...","expired_at":...}
    """
    data = _call_api(f"{API_BASE_URL}/create", None, "create")
    if not isinstance(data, dict):
        raise RuntimeError("smailpro: 创建响应格式异常")

    email = str(data.get("email") or "").strip()
    if not email:
        raise RuntimeError("smailpro: 创建邮箱失败, 未返回邮箱地址")

    # token 不参与后续逻辑，使用邮箱地址占位以满足上层非空校验
    return EmailInfo(
        channel=channel,
        email=email,
        _token=email,
        expires_at=data.get("expired_at"),
    )


def _fetch_message(email: str, mid: str) -> Tuple[str, str]:
    """获取单封邮件正文，返回 (html, text)；失败返回空串"""
    if not mid:
        return "", ""
    try:
        data = _call_api(
            f"{API_BASE_URL}/message",
            {"email": email, "mid": mid},
            "message",
        )
    except (RuntimeError, ValueError, OSError):
        return "", ""
    if not isinstance(data, dict):
        return "", ""
    return str(data.get("body") or ""), str(data.get("textBody") or "")


def get_emails(email: str, token: str = "") -> List[Email]:
    """
    获取 smailpro 邮件列表

    1. 调用 sonjj inbox 接口获取列表（仅含 mid、from、subject、datetime）
    2. 对每封邮件调用 message 接口补全正文，再统一标准化
    token 不参与逻辑，可忽略。
    """
    email = (email or "").strip()
    if not email:
        raise ValueError("smailpro: 邮箱地址为空")

    data = _call_api(f"{API_BASE_URL}/inbox", {"email": email}, "inbox")
    if not isinstance(data, dict):
        raise RuntimeError("smailpro: 邮件列表响应格式异常")

    mails = data.get("mails")
    if not isinstance(mails, list) or not mails:
        return []

    out: List[Email] = []
    for m in mails:
        if not isinstance(m, dict):
            continue
        mid = str(m.get("mid") or "").strip()
        html_body, text_body = _fetch_message(email, mid)
        raw = {
            "id": mid,
            "from": m.get("from", ""),
            "to": email,
            "subject": m.get("subject", ""),
            "date": m.get("datetime", ""),
        }
        # 拉取成功时补全正文，失败则保留列表元信息
        if html_body or text_body:
            raw["html"] = html_body
            raw["text"] = text_body
        out.append(normalize_email(raw, email))

    return out
