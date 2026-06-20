"""
Tempmailo 渠道 — https://tempmailo.com
"""

import json
import re
from typing import Any, Dict, List

import requests

from ..config import get_config
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "tempmailo"
BASE_URL = "https://tempmailo.com"
TOKEN_RE = re.compile(r'name="__RequestVerificationToken"[^>]*value="([^"]+)"', re.I)
TOKEN_RE_REV = re.compile(r'value="([^"]+)"[^>]*name="__RequestVerificationToken"', re.I)

HEADERS = {
    "Accept": "text/html,application/json,*/*;q=0.8",
    "Referer": f"{BASE_URL}/",
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
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


def _parse_verification_token(html: str) -> str:
    match = TOKEN_RE.search(html) or TOKEN_RE_REV.search(html)
    if not match:
        raise ValueError("tempmailo: verification token not found")
    return match.group(1)


def _cookie_header(sess: requests.Session) -> str:
    return "; ".join(f"{k}={v}" for k, v in sess.cookies.items())


def _decode_token(token: str) -> Dict[str, str]:
    try:
        data = json.loads(token)
    except Exception as exc:
        raise ValueError("tempmailo: invalid session token") from exc
    verification_token = str(data.get("verificationToken") or "").strip()
    cookie = str(data.get("cookie") or "").strip()
    if not verification_token or not cookie:
        raise ValueError("tempmailo: invalid session token")
    return {"verificationToken": verification_token, "cookie": cookie}


def generate_email() -> EmailInfo:
    sess = _session()
    config = get_config()
    home = sess.get(BASE_URL, timeout=config.timeout)
    home.raise_for_status()
    verification_token = _parse_verification_token(home.text)

    changed = sess.get(
        f"{BASE_URL}/changemail",
        params={"_r": "1"},
        headers={"RequestVerificationToken": verification_token, "Accept": "text/plain,*/*;q=0.8"},
        timeout=config.timeout,
    )
    changed.raise_for_status()
    email = changed.text.strip()
    if "@" not in email:
        raise ValueError("tempmailo: invalid email response")

    token = json.dumps({
        "verificationToken": verification_token,
        "cookie": _cookie_header(sess),
    })
    return EmailInfo(channel=CHANNEL, email=email, _token=token)


def get_emails(token: str, email: str) -> List[Email]:
    session_data = _decode_token(token)
    address = (email or "").strip()
    if not address:
        raise ValueError("tempmailo: empty email")

    resp = requests.post(
        BASE_URL,
        headers={
            **HEADERS,
            "Accept": "application/json,*/*;q=0.8",
            "Content-Type": "application/json",
            "Cookie": session_data["cookie"],
            "RequestVerificationToken": session_data["verificationToken"],
        },
        json={"mail": address},
        timeout=get_config().timeout,
        verify=not get_config().insecure,
    )
    resp.raise_for_status()
    data = resp.json()
    rows: List[Dict[str, Any]] = data if isinstance(data, list) else []
    return [normalize_email(raw, address) for raw in rows if isinstance(raw, dict)]
