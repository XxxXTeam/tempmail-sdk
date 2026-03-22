"""
mail.chatgpt.org.uk 渠道实现
API: https://mail.chatgpt.org.uk/api
"""

import json
import re
from urllib.parse import quote

from .. import http as tm_http
from ..types import EmailInfo
from ..normalize import normalize_email

CHANNEL = "chatgpt-org-uk"
BASE_URL = "https://mail.chatgpt.org.uk/api"
HOME_URL = "https://mail.chatgpt.org.uk/"

DEFAULT_HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36",
    "Accept": "*/*",
    "Referer": "https://mail.chatgpt.org.uk/",
    "Origin": "https://mail.chatgpt.org.uk",
    "DNT": "1",
}

_BROWSER_AUTH_RE = re.compile(
    r"__BROWSER_AUTH\s*=\s*(\{[\s\S]*?\})\s*;",
    re.MULTILINE,
)


def _extract_gm_sid(resp) -> str:
    for name, value in resp.cookies.items():
        if name == "gm_sid":
            return value
    return ""


def _extract_browser_auth(html: str) -> str:
    m = _BROWSER_AUTH_RE.search(html)
    if not m:
        return ""
    try:
        o = json.loads(m.group(1))
        t = o.get("token")
        return t if isinstance(t, str) else ""
    except Exception:
        return ""


def _fetch_home_session_once():
    resp = tm_http.get(
        HOME_URL,
        headers=DEFAULT_HEADERS,
    )
    resp.raise_for_status()
    gm_sid = _extract_gm_sid(resp)
    browser = _extract_browser_auth(resp.text or "")
    if not gm_sid:
        raise Exception("Failed to extract gm_sid cookie")
    if not browser:
        raise Exception("Failed to extract __BROWSER_AUTH from homepage (API now requires browser session)")
    return gm_sid, browser


def _fetch_home_session():
    try:
        return _fetch_home_session_once()
    except Exception as exc:
        msg = str(exc).lower()
        if "401" in msg or "extract" in msg or "gm_sid" in msg:
            return _fetch_home_session_once()
        raise


def _fetch_inbox_token_once(email: str, gm_sid: str) -> str:
    resp = tm_http.post(
        f"{BASE_URL}/inbox-token",
        headers={
            **DEFAULT_HEADERS,
            "Content-Type": "application/json",
            "Cookie": f"gm_sid={gm_sid}",
        },
        json={"email": email},
    )
    resp.raise_for_status()
    data = resp.json()
    token = data.get("auth", {}).get("token")
    if not token:
        raise Exception("Failed to get inbox token")
    return token


def _fetch_inbox_token(email: str, gm_sid: str) -> str:
    try:
        return _fetch_inbox_token_once(email, gm_sid)
    except Exception as exc:
        if "401" in str(exc):
            gm_sid2, _browser = _fetch_home_session()
            return _fetch_inbox_token_once(email, gm_sid2)
        raise


def _parse_packed_token(packed: str) -> tuple[str, str]:
    t = packed.strip()
    if t.startswith("{"):
        try:
            o = json.loads(t)
            gs = o.get("gmSid")
            ib = o.get("inbox")
            if isinstance(gs, str) and isinstance(ib, str):
                return gs, ib
        except Exception:
            pass
    return "", packed


def generate_email(**kwargs) -> EmailInfo:
    """创建临时邮箱"""
    gm_sid, browser_token = _fetch_home_session()

    resp = tm_http.get(
        f"{BASE_URL}/generate-email",
        headers={
            **DEFAULT_HEADERS,
            "Cookie": f"gm_sid={gm_sid}",
            "X-Inbox-Token": browser_token,
        },
    )
    resp.raise_for_status()
    data = resp.json()

    if not data.get("success"):
        raise Exception("Failed to generate email")

    email = data["data"]["email"]
    inbox = (data.get("auth") or {}).get("token") or _fetch_inbox_token(email, gm_sid)
    packed = json.dumps({"gmSid": gm_sid, "inbox": inbox}, separators=(",", ":"))

    return EmailInfo(
        channel=CHANNEL,
        email=email,
        _token=packed,
    )


def get_emails(token: str, email: str, **kwargs) -> list:
    """获取邮件列表"""
    if not token:
        raise Exception("internal error: token missing for chatgpt-org-uk")

    gm_sid, inbox = _parse_packed_token(token)
    if not gm_sid:
        gm_sid = _fetch_home_session()[0]

    def _fetch_emails(inbox_value: str, gm_sid_value: str) -> list:
        resp = tm_http.get(
            f"{BASE_URL}/emails?email={quote(email)}",
            headers={
                **DEFAULT_HEADERS,
                "Cookie": f"gm_sid={gm_sid_value}",
                "x-inbox-token": inbox_value,
            },
        )
        resp.raise_for_status()
        data = resp.json()

        if not data.get("success"):
            raise Exception("Failed to get emails")

        return [normalize_email(raw, email) for raw in (data.get("data", {}).get("emails") or [])]

    try:
        return _fetch_emails(inbox, gm_sid)
    except Exception as exc:
        if "401" in str(exc):
            gm_sid2, _b = _fetch_home_session()
            refreshed = _fetch_inbox_token(email, gm_sid2)
            return _fetch_emails(refreshed, gm_sid2)
        raise
