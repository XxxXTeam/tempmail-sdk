"""
apihz（接口盒子）渠道 — https://apihz.cn

需 id/key 凭据。内置官方公共账号 88888888/88888888（共享频次）作为默认，
可用环境变量 APIHZ_ID / APIHZ_KEY 或 set_config(apihz_id=..., apihz_key=...) 覆盖。

流程:
  1. GET /api/mail/mailcache.php?id=&key=&domain=&name=&pwd=&buytype=0 → 创建邮箱
     返回 {"code":200,"mail":"...","pwd":"...","endtime":"..."}，有效期 10 分钟
  2. GET /api/mail/mailgetlist.php?id=&key=&mail=&pwd=&page=1 → 读信（必须携带创建时的 pwd）
     返回 {"code":200,"num":"N","data":[{frommail,fromname,subject,time1,time2,content}]}

token 编码: "apihz1:" + base64(JSON{mail,pwd})，读信时解出 mail 与 pwd。
"""

import base64
import json
import random
import string
from typing import Any, Dict, List, Tuple

from ..config import get_config
from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "apihz"
BASE_URL = "https://cn.apihz.cn"
TOK_PREFIX = "apihz1:"

# apihz 官方公共账号（共享频次），未配置自有 id/key 时使用
PUBLIC_ID = "88888888"
PUBLIC_KEY = "88888888"

# 可用收信域名（apihz 自研，MX 指向 mail.apimail.* 自建服务器）
DOMAINS = ["apimail.email", "apimail.vip"]


def _get_ua() -> str:
    """获取 User-Agent，优先使用配置中的自定义 UA"""
    c = get_config()
    return (c.headers or {}).get("User-Agent") or (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36"
    )


def _get_credentials() -> Tuple[str, str]:
    """读取 apihz 调用凭据：优先配置/环境变量，回退官方公共账号"""
    cfg = get_config()
    cid = (getattr(cfg, "apihz_id", None) or "").strip() or PUBLIC_ID
    ckey = (getattr(cfg, "apihz_key", None) or "").strip() or PUBLIC_KEY
    return cid, ckey


def _random_local(n: int) -> str:
    """生成 n 位小写字母数字随机邮箱前缀"""
    chars = string.ascii_lowercase + string.digits
    return "".join(random.choice(chars) for _ in range(n))


def _random_password() -> str:
    """生成 12 位随机密码（读信必须携带创建时的密码）"""
    chars = string.ascii_letters + string.digits
    return "".join(random.choice(chars) for _ in range(12))


def _enc_token(mail: str, pwd: str) -> str:
    """token 编码：前缀 + base64(JSON{mail,pwd})，读信时解出 pwd"""
    raw = json.dumps({"mail": mail, "pwd": pwd}, ensure_ascii=False)
    return TOK_PREFIX + base64.b64encode(raw.encode("utf-8")).decode("ascii")


def _dec_token(tok: str) -> Tuple[str, str]:
    """token 解码：去前缀 → base64 解码 → JSON 解析，返回 (mail, pwd)"""
    if not tok or not tok.startswith(TOK_PREFIX):
        raise ValueError("apihz: 无效 token")
    try:
        raw = base64.b64decode(tok[len(TOK_PREFIX) :]).decode("utf-8")
        obj = json.loads(raw)
    except Exception as exc:
        raise ValueError("apihz: 无效 token") from exc
    mail = str(obj.get("mail") or "").strip()
    pwd = str(obj.get("pwd") or "").strip()
    if not mail or not pwd:
        raise ValueError("apihz: 无效 token")
    return mail, pwd


def _first(msg: Dict[str, Any], *keys: str) -> str:
    """按优先级从消息字典中提取首个非空值并转为字符串"""
    for key in keys:
        val = msg.get(key)
        if val is not None and val != "":
            return str(val)
    return ""


def generate_email() -> EmailInfo:
    """创建 apihz（接口盒子）临时邮箱，有效期 10 分钟"""
    cid, ckey = _get_credentials()
    domain = random.choice(DOMAINS)
    name = _random_local(10)
    pwd = _random_password()

    resp = tm_http.get(
        f"{BASE_URL}/api/mail/mailcache.php",
        headers={"User-Agent": _get_ua(), "Accept": "application/json"},
        params={
            "id": cid,
            "key": ckey,
            "domain": domain,
            "name": name,
            "pwd": pwd,
            "buytype": "0",
        },
    )
    resp.raise_for_status()

    data = resp.json()
    if not isinstance(data, dict) or data.get("code") != 200 or not data.get("mail"):
        raise RuntimeError(f"apihz: 创建邮箱失败 {data!r}")

    mail = str(data["mail"])
    # 优先使用响应回传的 pwd（与请求一致），确保读信密码正确
    final_pwd = str(data.get("pwd") or pwd)

    return EmailInfo(channel=CHANNEL, email=mail, _token=_enc_token(mail, final_pwd))


def get_emails(token: str) -> List[Email]:
    """获取 apihz 邮件列表，需携带创建时的 mail 与 pwd（编码在 token 中）"""
    mail, pwd = _dec_token(token)
    cid, ckey = _get_credentials()

    resp = tm_http.get(
        f"{BASE_URL}/api/mail/mailgetlist.php",
        headers={"User-Agent": _get_ua(), "Accept": "application/json"},
        params={"id": cid, "key": ckey, "mail": mail, "pwd": pwd, "page": "1"},
    )
    resp.raise_for_status()

    data = resp.json()
    if not isinstance(data, dict) or data.get("code") != 200:
        return []

    items = data.get("data")
    if not isinstance(items, list):
        return []

    out: List[Email] = []
    for msg in items:
        if not isinstance(msg, dict):
            continue
        row = {
            "id": _first(msg, "time1"),
            "from": _first(msg, "frommail", "fromname"),
            "to": mail,
            "subject": _first(msg, "subject"),
            "text": _first(msg, "content"),
            "html": _first(msg, "content"),
            "date": _first(msg, "time2", "time1"),
        }
        out.append(normalize_email(row, mail))
    return out
