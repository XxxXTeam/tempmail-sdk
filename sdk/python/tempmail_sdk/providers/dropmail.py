import json
import os
import threading
import time
from typing import Any, Dict, Optional, Tuple

from .. import http as tm_http
from ..config import get_config
from ..types import EmailInfo
from ..normalize import normalize_email

CHANNEL = "dropmail"

TOKEN_GENERATE_URL = "https://dropmail.me/api/token/generate"
TOKEN_RENEW_URL = "https://dropmail.me/api/token/renew"

CREATE_SESSION_QUERY = "mutation {introduceSession {id, expiresAt, addresses{id, address}}}"

GET_MAILS_QUERY = """query ($id: ID!) {
  session(id:$id) {
    mails {
      id, rawSize, fromAddr, toAddr, receivedAt,
      text, headerFrom, headerSubject, html
    }
  }
}"""

_TOKEN_HEADERS = {
    "Accept": "application/json",
    "Content-Type": "application/json",
    "Origin": "https://dropmail.me",
    "Referer": "https://dropmail.me/api/",
}

_AUTO_CACHE_SEC = 50 * 60
_RENEW_BEFORE_SEC = 10 * 60
_lock = threading.Lock()
_cached_af: Optional[Tuple[str, float]] = None  # (token, expiry monotonic)


def _explicit_af_token() -> str:
    cfg = get_config()
    t = (getattr(cfg, "dropmail_auth_token", None) or "").strip()
    if t:
        return t
    return (
        os.environ.get("DROPMAIL_AUTH_TOKEN", "").strip()
        or os.environ.get("DROPMAIL_API_TOKEN", "").strip()
    )


def _auto_token_disabled() -> bool:
    if getattr(get_config(), "dropmail_disable_auto_token", False):
        return True
    v = os.environ.get("DROPMAIL_NO_AUTO_TOKEN", "").strip().lower()
    return v in ("1", "true", "yes")


def _renew_lifetime() -> str:
    cfg = get_config()
    s = (getattr(cfg, "dropmail_renew_lifetime", None) or "").strip()
    if s:
        return s
    s = os.environ.get("DROPMAIL_RENEW_LIFETIME", "").strip()
    return s or "1d"


def _cache_seconds_for_lifetime(lifetime: str) -> int:
    s = lifetime.strip().lower()
    if s == "1h":
        return 50 * 60
    if s == "1d":
        return 23 * 60 * 60
    if s.endswith("d") and s[:-1].isdigit():
        days = int(s[:-1])
        return max(0, days - 1) * 24 * 60 * 60
    return _AUTO_CACHE_SEC


def _post_json(url: str, payload: dict) -> dict:
    resp = tm_http.post(
        url,
        data=json.dumps(payload),
        headers=_TOKEN_HEADERS,
    )
    resp.raise_for_status()
    return resp.json()


def _fetch_af_token() -> str:
    body = _post_json(TOKEN_GENERATE_URL, {"type": "af", "lifetime": "1h"})
    tok = str(body.get("token") or "").strip()
    if not tok or not tok.startswith("af_"):
        raise Exception(body.get("error") or "DropMail token/generate 未返回有效 af_ 令牌")
    return tok


def _renew_af_token(current: str, lifetime: str) -> str:
    body = _post_json(TOKEN_RENEW_URL, {"token": current, "lifetime": lifetime})
    tok = str(body.get("token") or "").strip()
    if not tok or not tok.startswith("af_"):
        raise Exception(body.get("error") or "DropMail token/renew 未返回有效 af_ 令牌")
    return tok


def _resolve_af_token() -> str:
    ex = _explicit_af_token()
    if ex:
        return ex
    if _auto_token_disabled():
        raise Exception(
            "DropMail 已禁用自动令牌：请设置 DROPMAIL_AUTH_TOKEN 或 "
            "set_config(dropmail_auth_token='af_...')，见 https://dropmail.me/api/"
        )

    now = time.monotonic()
    renew_life = _renew_lifetime()

    with _lock:
        global _cached_af
        if _cached_af is not None:
            tok, exp = _cached_af
            if now < exp - _RENEW_BEFORE_SEC:
                return tok

        if _cached_af is not None:
            old_tok, _ = _cached_af
            try:
                new_tok = _renew_af_token(old_tok, renew_life)
                _cached_af = (
                    new_tok,
                    time.monotonic() + _cache_seconds_for_lifetime(renew_life),
                )
                return new_tok
            except Exception:
                _cached_af = None

        new_tok = _fetch_af_token()
        _cached_af = (new_tok, time.monotonic() + _AUTO_CACHE_SEC)
        return new_tok


def _graphql_url() -> str:
    from urllib.parse import quote

    af = _resolve_af_token()
    return f"https://dropmail.me/api/graphql/{quote(af, safe='')}"


def _graphql_request(query: str, variables: dict = None) -> dict:
    data: Dict[str, Any] = {"query": query}
    if variables:
        data["variables"] = json.dumps(variables)

    resp = tm_http.post(
        _graphql_url(),
        data=data,
        headers={"Content-Type": "application/x-www-form-urlencoded"},
    )
    resp.raise_for_status()
    result = resp.json()

    if result.get("errors"):
        raise Exception(f"GraphQL error: {result['errors'][0].get('message', 'Unknown')}")

    return result.get("data", {})


def _flatten_message(mail: dict, recipient_email: str) -> dict:
    """将 dropmail 消息格式扁平化"""
    return {
        "id": mail.get("id", ""),
        "from": mail.get("fromAddr", ""),
        "to": mail.get("toAddr", recipient_email),
        "subject": mail.get("headerSubject", ""),
        "text": mail.get("text", ""),
        "html": mail.get("html", ""),
        "received_at": mail.get("receivedAt", ""),
        "attachments": [],
    }


def generate_email(**kwargs) -> EmailInfo:
    """创建临时邮箱"""
    data = _graphql_request(CREATE_SESSION_QUERY)
    session = data.get("introduceSession")

    if not session or not session.get("addresses"):
        raise Exception("Failed to generate email")

    return EmailInfo(
        channel=CHANNEL,
        email=session["addresses"][0]["address"],
        _token=session["id"],
        expires_at=session.get("expiresAt"),
    )


def get_emails(token: str, email: str = "", **kwargs) -> list:
    """获取邮件列表"""
    data = _graphql_request(GET_MAILS_QUERY, {"id": token})
    mails = (data.get("session") or {}).get("mails") or []
    return [normalize_email(_flatten_message(m, email), email) for m in mails]
