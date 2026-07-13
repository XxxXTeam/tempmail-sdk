"""
vip.215.im：POST /api/temp-inbox 创建收件箱；收件通过 WebSocket message.new 推送
推送无正文时统一合成 text/html（synthetic-v1），与 Node/Go/Rust 对齐。
"""

import html
import json
import threading
import time
from typing import Dict, List, Optional
from urllib.parse import quote

import websocket

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "vip-215"
BASE = "https://vip.215.im"

USER_AGENT = (
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/148.0.0.0 Safari/537.36 Edg/148.0.0.0"
)

HOME_HEADERS = {
    "User-Agent": USER_AGENT,
    "Accept": (
        "text/html,application/xhtml+xml,application/xml;q=0.9,"
        "image/avif,image/webp,image/apng,*/*;q=0.8"
    ),
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "Cache-Control": "no-cache",
    "DNT": "1",
    "Pragma": "no-cache",
    "Sec-CH-UA": '"Chromium";v="148", "Microsoft Edge";v="148", "Not/A)Brand";v="99"',
    "Sec-CH-UA-Mobile": "?0",
    "Sec-CH-UA-Platform": '"Windows"',
    "Sec-Fetch-Dest": "document",
    "Sec-Fetch-Mode": "navigate",
    "Sec-Fetch-Site": "same-origin",
    "Sec-Fetch-User": "?1",
    "Upgrade-Insecure-Requests": "1",
}

HEADERS = {
    "User-Agent": USER_AGENT,
    "Accept": "*/*",
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "Cache-Control": "no-cache",
    "Content-Type": "application/json",
    "DNT": "1",
    "Origin": BASE,
    "Pragma": "no-cache",
    "Referer": f"{BASE}/",
    "Sec-CH-UA": '"Chromium";v="148", "Microsoft Edge";v="148", "Not/A)Brand";v="99"',
    "Sec-CH-UA-Mobile": "?0",
    "Sec-CH-UA-Platform": '"Windows"',
    "Sec-Fetch-Dest": "empty",
    "Sec-Fetch-Mode": "cors",
    "Sec-Fetch-Site": "same-origin",
    "X-Locale": "zh",
}

WS_EXTRA_HEADERS = [
    f"User-Agent: {USER_AGENT}",
    f"Origin: {BASE}",
]


def _cookie_header_from_response(resp) -> str:
    jar = getattr(resp, "cookies", None)
    if jar:
        return "; ".join(f"{c.name}={c.value}" for c in jar)
    raw = resp.headers.get("Set-Cookie") or ""
    parts = []
    for segment in raw.split(","):
        nv = segment.split(";")[0].strip()
        if nv and "=" in nv:
            parts.append(nv)
    return "; ".join(parts)


def _establish_session() -> str:
    resp = tm_http.get(f"{BASE}/", headers=HOME_HEADERS, timeout=15)
    resp.raise_for_status()
    cookie = _cookie_header_from_response(resp)
    if "yyds_homepage_bridge=" not in cookie or "yyds_homepage_device=" not in cookie:
        raise RuntimeError("vip-215: missing homepage cookies")
    return cookie


def _fetch_ws_ticket(jwt: str) -> str:
    resp = tm_http.get(
        f"{BASE}/v1/auth/ws-ticket",
        headers={**HEADERS, "Authorization": f"Bearer {jwt}"},
        timeout=15,
    )
    resp.raise_for_status()
    body = resp.json()
    if not body.get("success"):
        raise RuntimeError("vip-215: ws-ticket success=false")
    ticket = (body.get("data") or {}).get("ticket")
    if not ticket:
        raise RuntimeError("vip-215: missing ws ticket")
    return ticket


_SYNTHETIC_MARKER = "【tempmail-sdk|synthetic|vip-215|v1】"

_lock = threading.Lock()
_boxes: Dict[str, "_Vip215Box"] = {}


class _Vip215Box:
    __slots__ = ("emails", "by_id", "thread", "started", "recipient")

    def __init__(self, recipient: str) -> None:
        self.emails: List[Email] = []
        self.by_id: Dict[str, Email] = {}
        self.thread: Optional[threading.Thread] = None
        self.started = False
        self.recipient = recipient


def _sanitize_one_line(val) -> str:
    if val is None:
        return ""
    s = str(val).replace("\r\n", " ").replace("\r", " ").replace("\n", " ")
    return s.strip()


def _build_synthetic_bodies(recipient_email: str, data: dict) -> tuple[str, str]:
    eid = _sanitize_one_line(data.get("id"))
    subj = _sanitize_one_line(data.get("subject"))
    from_ = _sanitize_one_line(data.get("from"))
    to = _sanitize_one_line(recipient_email)
    date = _sanitize_one_line(data.get("date"))
    size = data.get("size")
    lines = [
        _SYNTHETIC_MARKER,
        f"id: {eid}",
        f"subject: {subj}",
        f"from: {from_}",
        f"to: {to}",
        f"date: {date}",
    ]
    if isinstance(size, (int, float)) and not isinstance(size, bool) and size >= 0:
        lines.append(f"size: {int(size)}")
    text = "\n".join(lines)

    pairs = [
        ("id", eid),
        ("subject", subj),
        ("from", from_),
        ("to", to),
        ("date", date),
    ]
    parts = []
    for k, v in pairs:
        parts.append(f"<dt>{html.escape(k)}</dt><dd>{html.escape(v)}</dd>")
    if isinstance(size, (int, float)) and not isinstance(size, bool) and size >= 0:
        sv = str(int(size))
        parts.append(f"<dt>size</dt><dd>{html.escape(sv)}</dd>")
    inner = "".join(parts)
    html_body = (
        '<div class="tempmail-sdk-synthetic" data-tempmail-sdk-format="synthetic-v1" '
        'data-channel="vip-215">'
        f'<dl class="tempmail-sdk-meta">{inner}</dl></div>'
    )
    return text, html_body


def _get_box(token: str, recipient: str) -> _Vip215Box:
    with _lock:
        b = _boxes.get(token)
        if b is None:
            b = _Vip215Box(recipient)
            _boxes[token] = b
        return b


def _ws_run(jwt: str, box: _Vip215Box) -> None:
    ws_ticket = _fetch_ws_ticket(jwt)
    url = f"wss://vip.215.im/v1/ws?token={quote(ws_ticket, safe='')}"

    def on_message(_ws, message: str) -> None:
        try:
            msg = json.loads(message)
        except (json.JSONDecodeError, TypeError):
            return
        if msg.get("type") != "message.new":
            return
        data = msg.get("data") or {}
        syn_text, syn_html = _build_synthetic_bodies(box.recipient, data)
        raw = {
            "id": data.get("id"),
            "from": data.get("from"),
            "subject": data.get("subject"),
            "date": data.get("date"),
            "to": box.recipient,
            "text": syn_text,
            "html": syn_html,
        }
        em = normalize_email(raw, box.recipient)
        if not em.id:
            return
        with _lock:
            if em.id in box.by_id:
                return
            box.by_id[em.id] = em
            box.emails.append(em)

    ws_app = websocket.WebSocketApp(
        url,
        header=WS_EXTRA_HEADERS,
        on_message=on_message,
    )
    ws_app.run_forever(ping_interval=60, ping_timeout=30)


def _ensure_ws(token: str, recipient: str) -> None:
    box = _get_box(token, recipient)
    with _lock:
        if box.started:
            return
        box.started = True
        t = threading.Thread(target=_ws_run, args=(token, box), daemon=True)
        box.thread = t
        t.start()
    # 给握手留一点时间，避免首帧竞态
    time.sleep(0.08)


def generate_email() -> EmailInfo:
    cookie = _establish_session()
    hdrs = {**HEADERS, "Cookie": cookie}
    resp = tm_http.post(
        f"{BASE}/api/temp-inbox",
        headers=hdrs,
        data=b"",
        timeout=15,
    )
    resp.raise_for_status()
    body = resp.json()
    if not body.get("success"):
        raise RuntimeError("vip-215: success=false")
    data = body.get("data") or {}
    address = data.get("address")
    tok = data.get("token")
    if not address or not tok:
        raise RuntimeError("vip-215: missing address or token")
    _ensure_ws(tok, address)
    return EmailInfo(
        channel=CHANNEL,
        email=address,
        _token=tok,
        created_at=data.get("createdAt"),
        expires_at=None,
    )


def get_emails(token: str, email: str) -> List[Email]:
    _ensure_ws(token, email)
    box = _get_box(token, email)
    with _lock:
        return list(box.emails)
