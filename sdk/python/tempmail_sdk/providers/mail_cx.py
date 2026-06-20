"""
mail.cx 渠道实现
匿名 Web API: https://mail.cx/v1
"""

import random
import string
from datetime import datetime, timedelta, timezone
from typing import Any, Dict, List, Optional
from urllib.parse import quote

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "mail-cx"
BASE_URL = "https://mail.cx"


def _random_string(length: int) -> str:
    chars = string.ascii_lowercase + string.digits
    return "".join(random.choice(chars) for _ in range(length))


def _client_id() -> str:
    return "tempmail-sdk-" + _random_string(16)


def _headers(client_id: str) -> Dict[str, str]:
    return {
        "Accept": "application/json",
        "Origin": BASE_URL,
        "Referer": BASE_URL + "/",
        "User-Agent": (
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
            "AppleWebKit/537.36 (KHTML, like Gecko) "
            "Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
        ),
        "X-Client-ID": client_id,
    }


def _get_config(client_id: str) -> Dict[str, Any]:
    resp = tm_http.get(f"{BASE_URL}/v1/config", headers=_headers(client_id), timeout=15)
    resp.raise_for_status()
    data = resp.json()
    return data if isinstance(data, dict) else {}


def _pick_domain(config: Dict[str, Any], preferred_domain: Optional[str]) -> str:
    raw_domains = config.get("system_domains")
    domain_items = raw_domains if isinstance(raw_domains, list) else []
    domains = [
        str(item.get("domain", "")).strip()
        for item in domain_items
        if isinstance(item, dict) and str(item.get("domain", "")).strip()
    ]
    if not domains:
        raise ValueError("mail-cx: no system domains")

    preferred = str(preferred_domain or "").strip().removeprefix("@").lower()
    if preferred:
        for domain in domains:
            if domain.lower() == preferred:
                return domain

    for item in domain_items:
        if isinstance(item, dict) and item.get("default") and str(item.get("domain", "")).strip():
            return str(item["domain"]).strip()
    return domains[0]


def _first_non_empty(*values: Any) -> str:
    for value in values:
        if value is None:
            continue
        text = str(value).strip()
        if text:
            return text
    return ""


def _flatten_list_message(raw: Dict[str, Any], recipient: str) -> Dict[str, Any]:
    return {
        "id": raw.get("id"),
        "from": _first_non_empty(raw.get("from_email"), raw.get("from_name")),
        "to": recipient,
        "subject": raw.get("subject", ""),
        "text": raw.get("preview_text", ""),
        "created_at": raw.get("created_at"),
        "attachments": raw.get("attachments"),
    }


def _flatten_detail(raw: Dict[str, Any], recipient: str) -> Dict[str, Any]:
    return {
        "id": raw.get("id"),
        "from": _first_non_empty(raw.get("from_email"), raw.get("from_name")),
        "to": recipient,
        "subject": raw.get("subject", ""),
        "text_body": raw.get("text_body"),
        "html_body": raw.get("html_body"),
        "text": _first_non_empty(raw.get("text_body"), raw.get("preview_text")),
        "html": raw.get("html_body", ""),
        "created_at": raw.get("created_at"),
        "attachments": raw.get("attachments"),
    }


def _get_detail(client_id: str, message_id: str) -> Optional[Dict[str, Any]]:
    resp = tm_http.get(
        f"{BASE_URL}/v1/email/{quote(message_id, safe='')}",
        headers=_headers(client_id),
        timeout=15,
    )
    if not resp.ok:
        return None
    data = resp.json()
    return data if isinstance(data, dict) else None


def generate_email(preferred_domain: Optional[str] = None) -> EmailInfo:
    """创建 mail.cx 临时邮箱"""
    client_id = _client_id()
    config = _get_config(client_id)
    domain = _pick_domain(config, preferred_domain)
    created_at = datetime.now(timezone.utc)

    expires_at = None
    ttl = config.get("ttl_seconds")
    if isinstance(ttl, (int, float)) and ttl > 0:
        expires_at = int((created_at + timedelta(seconds=ttl)).timestamp() * 1000)

    return EmailInfo(
        channel=CHANNEL,
        email=f"{_random_string(12)}@{domain}",
        _token=client_id,
        expires_at=expires_at,
        created_at=created_at.isoformat(),
    )


def get_emails(client_id: str, email: str) -> List[Email]:
    """获取 mail.cx 邮件列表"""
    resp = tm_http.get(
        f"{BASE_URL}/v1/inbox/{quote(email, safe='')}",
        headers=_headers(client_id),
        timeout=30,
    )
    if resp.status_code == 204:
        return []
    resp.raise_for_status()

    data = resp.json()
    rows = data.get("emails") if isinstance(data, dict) else []
    if not isinstance(rows, list):
        return []

    emails: List[Email] = []
    for row in rows:
        if not isinstance(row, dict):
            emails.append(normalize_email({}, email))
            continue
        raw = _flatten_list_message(row, email)
        message_id = str(row.get("id", "")).strip()
        if message_id:
            try:
                detail = _get_detail(client_id, message_id)
                if detail:
                    raw = _flatten_detail(detail, email)
            except Exception:
                pass
        emails.append(normalize_email(raw, email))
    return emails
