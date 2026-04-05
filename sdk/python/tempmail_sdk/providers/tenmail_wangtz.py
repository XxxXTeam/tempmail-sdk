"""
10mail.wangtz.cn 临时邮箱
POST /api/tempMail、/api/emailList；域名固定为 wangtz.cn。
"""

from typing import Any, Dict, List, Optional

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import EmailInfo

CHANNEL = "10mail-wangtz"
ORIGIN = "https://10mail.wangtz.cn"
MAIL_DOMAIN = "wangtz.cn"

_HEADERS = {
    "Accept": "*/*",
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "Content-Type": "application/json; charset=utf-8",
    "Origin": ORIGIN,
    "Referer": f"{ORIGIN}/",
    "token": "null",
    "Authorization": "null",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
}


def _local_part(domain: Optional[str]) -> str:
    if not domain:
        return ""
    s = str(domain).strip()
    if "@" in s:
        return s.split("@", 1)[0].strip()
    return s


def _format_addr_field(v: Any) -> str:
    """解析 emailList 中 mailparser 风格地址：{ text, value: [{address, name}] }"""
    if not isinstance(v, dict):
        return ""
    t = v.get("text")
    if t is not None and str(t).strip():
        return str(t).strip()
    val = v.get("value")
    if not isinstance(val, list) or not val:
        return ""
    first = val[0]
    if not isinstance(first, dict):
        return ""
    addr = str(first.get("address") or "").strip()
    if not addr:
        return ""
    name = str(first.get("name") or "").strip()
    if name and name.lower() != addr.lower():
        return f"{name} <{addr}>"
    return addr


def _flatten_message(raw: Dict[str, Any]) -> Dict[str, Any]:
    out = dict(raw)
    mid = raw.get("messageId")
    if mid is not None:
        out["id"] = mid
    fs = _format_addr_field(raw.get("from"))
    if fs:
        out["from"] = fs
    ts = _format_addr_field(raw.get("to"))
    if ts:
        out["to"] = ts
    return out


def generate_email(domain: Optional[str] = None, **kwargs) -> EmailInfo:
    """POST /api/tempMail；domain 为可选本地部分（emailName）。"""
    # 该渠道默认跳过 TLS 证书校验（与 curl --insecure 一致）
    resp = tm_http.post(
        f"{ORIGIN}/api/tempMail",
        headers=_HEADERS,
        json={"emailName": _local_part(domain)},
        verify=False,
    )
    resp.raise_for_status()
    data = resp.json()
    if data.get("code") != 0 or not data.get("data"):
        msg = data.get("msg") or "create failed"
        raise RuntimeError(f"10mail-wangtz: {msg}")
    d = data["data"]
    local = str(d.get("mailName") or "").strip()
    if not local:
        raise RuntimeError("10mail-wangtz: empty mailName")
    email = f"{local}@{MAIL_DOMAIN}"
    end = d.get("endTime")
    expires_at = int(end) if isinstance(end, (int, float)) else None
    return EmailInfo(channel=CHANNEL, email=email, _token="", expires_at=expires_at)


def get_emails(email: str, token: str = "", **kwargs) -> List:
    """POST /api/emailList；无需 token。"""
    resp = tm_http.post(
        f"{ORIGIN}/api/emailList",
        headers=_HEADERS,
        json={"emailName": email.strip()},
        verify=False,
    )
    resp.raise_for_status()
    raw_list = resp.json()
    if not isinstance(raw_list, list):
        raise RuntimeError("10mail-wangtz: inbox response is not a list")
    out = []
    for item in raw_list:
        if isinstance(item, dict):
            out.append(normalize_email(_flatten_message(item), email))
        else:
            out.append(normalize_email({}, email))
    return out
