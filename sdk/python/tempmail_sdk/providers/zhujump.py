"""
mail.zhujump.com 固定域渠道
通过注册账号、登录会话后创建固定域邮箱，再通过邮箱 ID 拉取邮件列表。
"""

import base64
import json
import secrets
import string
from typing import Dict, List

import requests

from ..config import get_config
from .. import http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

BASE_URL = "https://mail.zhujump.com"
LOGIN_REFERER = f"{BASE_URL}/zh-CN/login"
TOKEN_PREFIX = "zhj1:"
DEFAULT_EXPIRY_TIME = 60 * 60 * 1000

HEADERS = {
    "Accept": "application/json",
    "Origin": BASE_URL,
    "Referer": LOGIN_REFERER,
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36"
    ),
}


def _session() -> requests.Session:
    config = get_config()
    sess = requests.Session()
    sess.headers.update(HEADERS)
    if config.proxy:
        sess.proxies = {"http": config.proxy, "https": config.proxy}
    sess.verify = not config.insecure
    if config.headers:
        sess.headers.update(config.headers)
    return sess


def _random_value(prefix: str, size: int) -> str:
    chars = string.ascii_lowercase + string.digits
    return prefix + "".join(secrets.choice(chars) for _ in range(size))


def _random_password() -> str:
    return "Sdk!" + _random_value("", 12) + "A1"


def _login_referer(base_url: str) -> str:
    return f"{base_url}/zh-CN/login"


def _headers(base_url: str) -> Dict[str, str]:
    return {**HEADERS, "Origin": base_url, "Referer": _login_referer(base_url)}


def _encode_token(cookie: str, email_id: str, base_url: str) -> str:
    raw = json.dumps({"c": cookie, "i": email_id, "b": base_url}, separators=(",", ":")).encode("utf-8")
    return TOKEN_PREFIX + base64.standard_b64encode(raw).decode("ascii")


def _decode_token(token: str) -> Dict[str, str]:
    if not token.startswith(TOKEN_PREFIX):
        raise ValueError("zhujump: invalid session token")
    try:
        raw = base64.standard_b64decode(token[len(TOKEN_PREFIX):].encode("ascii"))
        data = json.loads(raw.decode("utf-8"))
    except Exception as exc:
        raise ValueError("zhujump: invalid session token") from exc
    cookie = str(data.get("c") or "").strip()
    email_id = str(data.get("i") or "").strip()
    if not cookie or not email_id:
        raise ValueError("zhujump: invalid session token")
    base_url = str(data.get("b") or BASE_URL).strip().rstrip("/")
    return {"cookie": cookie, "email_id": email_id, "base_url": base_url}


def generate_email(domain: str, channel: str) -> EmailInfo:
    return generate_email_for_instance(BASE_URL, domain, channel, None)


def generate_email_for_instance(base_url: str, domain: str, channel: str, expiry_time: int | None = DEFAULT_EXPIRY_TIME) -> EmailInfo:
    base_url = base_url.rstrip("/")
    sess = _session()
    sess.headers.update(_headers(base_url))
    config = get_config()
    username = _random_value("sdk", 10)
    password = _random_password()

    reg = sess.post(
        f"{base_url}/api/auth/register",
        json={"username": username, "password": password, "turnstileToken": ""},
        timeout=config.timeout,
    )
    reg.raise_for_status()

    csrf_resp = sess.get(f"{base_url}/api/auth/csrf", timeout=config.timeout)
    csrf_resp.raise_for_status()
    csrf = str((csrf_resp.json() or {}).get("csrfToken") or "").strip()
    if not csrf:
        raise ValueError("zhujump: csrf token missing")

    login = sess.post(
        f"{base_url}/api/auth/callback/credentials?",
        data={
            "username": username,
            "password": password,
            "turnstileToken": "",
            "redirect": "false",
            "csrfToken": csrf,
            "callbackUrl": _login_referer(base_url),
        },
        headers={"x-auth-return-redirect": "1"},
        timeout=config.timeout,
        allow_redirects=False,
    )
    login.raise_for_status()

    session_resp = sess.get(f"{base_url}/api/auth/session", timeout=config.timeout)
    session_resp.raise_for_status()
    session_json = session_resp.json() or {}
    if str((((session_json.get("user") or {}).get("username")) or "").strip()) != username:
        raise ValueError("zhujump: login verification failed")

    local = _random_value("sdk", 10)
    body: Dict[str, object] = {"name": local, "domain": domain}
    if expiry_time is not None:
        body["expiryTime"] = expiry_time
    created = sess.post(
        f"{base_url}/api/emails/generate",
        json=body,
        timeout=config.timeout,
    )
    created.raise_for_status()
    created_json = created.json() or {}
    email = str(created_json.get("email") or "").strip()
    email_id = str(created_json.get("id") or "").strip()
    if not email or not email_id:
        raise ValueError("zhujump: invalid generate response")

    cookie = "; ".join(f"{k}={v}" for k, v in sess.cookies.items())
    return EmailInfo(channel=channel, email=email, _token=_encode_token(cookie, email_id, base_url))


def _fetch_detail(base_url: str, cookie: str, email_id: str, message_id: str, config) -> Dict[str, object] | None:
    resp = http.get(
        f"{base_url}/api/emails/{email_id}/{message_id}",
        headers={**_headers(base_url), "Cookie": cookie},
    )
    if not resp.ok:
        return None
    data = resp.json() or {}
    message = data.get("message") if isinstance(data, dict) else None
    return message if isinstance(message, dict) else None


def get_emails(token: str, email: str) -> List[Email]:
    session = _decode_token(token)
    config = get_config()
    base_url = session["base_url"]
    resp = http.get(
        f"{base_url}/api/emails/{session['email_id']}",
        headers={**_headers(base_url), "Cookie": session["cookie"]},
    )
    resp.raise_for_status()
    data = resp.json() or {}
    rows = data.get("messages") if isinstance(data, dict) else []
    if not isinstance(rows, list):
        return []

    emails: List[Email] = []
    for item in rows:
        if not isinstance(item, dict):
            continue
        source = item
        message_id = str(item.get("id") or "").strip()
        if message_id and not (str(item.get("content") or "").strip() or str(item.get("html") or "").strip()):
            detail = _fetch_detail(base_url, session["cookie"], session["email_id"], message_id, config)
            if detail:
                source = {**item, **detail}
        emails.append(normalize_email({
            "id": source.get("id") or "",
            "from_address": source.get("from_address") or "",
            "to_address": source.get("to_address") or email,
            "subject": source.get("subject") or "",
            "content": source.get("content") or "",
            "html": source.get("html") or "",
            "received_at": source.get("received_at") or source.get("sent_at"),
            "isRead": False,
        }, email))
    return emails
