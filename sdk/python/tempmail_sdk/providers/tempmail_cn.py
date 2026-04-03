"""
tempmail.cn：按 public 前端的 Socket.IO 事件协议工作
- `request shortid` -> `shortid`
- `set shortid` 持续订阅 `mail`
"""

import json
import threading
import time
from typing import Dict, List, Optional

import websocket  # pyright: ignore[reportMissingImports]

from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "tempmail-cn"
DEFAULT_HOST = "tempmail.cn"
SOCKET_IO_VERSIONS = (4, 3)

HEADERS = {
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    ),
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "Cache-Control": "no-cache",
    "DNT": "1",
    "Pragma": "no-cache",
}

_lock = threading.Lock()
_boxes: Dict[str, "_TempmailCnBox"] = {}


class _TempmailCnBox:
    __slots__ = ("emails", "by_id", "thread", "started")

    def __init__(self) -> None:
        self.emails: List[Email] = []
        self.by_id: Dict[str, Email] = {}
        self.thread: Optional[threading.Thread] = None
        self.started = False


def _get_box(email: str) -> _TempmailCnBox:
    key = email.strip().lower()
    with _lock:
        box = _boxes.get(key)
        if box is None:
            box = _TempmailCnBox()
            _boxes[key] = box
        return box


def _normalize_host(domain: Optional[str]) -> str:
    raw = (domain or "").strip()
    if not raw:
        return DEFAULT_HOST

    host = raw
    lower = host.lower()
    if lower.startswith("http://") or lower.startswith("https://"):
        host = host.split("://", 1)[1]
    if "@" in host:
        host = host.rsplit("@", 1)[1]
    if "/" in host:
        host = host.split("/", 1)[0]
    host = host.strip(".")
    return host or DEFAULT_HOST


def _split_email(email: str) -> tuple[str, str]:
    trimmed = email.strip()
    if "@" not in trimmed:
        raise ValueError("tempmail-cn: invalid email address")
    local, host = trimmed.split("@", 1)
    if not local or not host:
        raise ValueError("tempmail-cn: invalid email address")
    return local, _normalize_host(host)


def _socket_headers(host: str) -> list[str]:
    return [
        f"User-Agent: {HEADERS['User-Agent']}",
        f"Origin: https://{host}",
        f"Referer: https://{host}/",
        f"Accept-Language: {HEADERS['Accept-Language']}",
        "Cache-Control: no-cache",
        "Pragma: no-cache",
        "DNT: 1",
    ]


def _connect_socket(host: str):
    last_err: Optional[Exception] = None
    for version in SOCKET_IO_VERSIONS:
        url = f"wss://{host}/socket.io/?EIO={version}&transport=websocket"
        ws = None
        try:
            ws = websocket.create_connection(
                url,
                header=_socket_headers(host),
                timeout=15,
                enable_multithread=True,
            )
            packet = ws.recv()
            if isinstance(packet, bytes):
                packet = packet.decode("utf-8", errors="ignore")
            if not isinstance(packet, str) or not packet.startswith("0"):
                raise RuntimeError(f"tempmail-cn: unexpected open packet for EIO={version}")
            ws.send("40")
            return ws
        except Exception as exc:  # noqa: BLE001
            last_err = exc
            if ws is not None:
                try:
                    ws.close()
                except Exception:  # noqa: BLE001
                    pass
    if last_err is None:
        raise RuntimeError("tempmail-cn: websocket handshake failed")
    raise RuntimeError(str(last_err))


def _send_event(ws, event: str, payload) -> None:
    packet = "42" + json.dumps([event, payload], ensure_ascii=False, separators=(",", ":"))
    ws.send(packet)


def _parse_event(packet: str):
    if not packet.startswith("42"):
        return None
    try:
        decoded = json.loads(packet[2:])
    except json.JSONDecodeError:
        return None
    if not isinstance(decoded, list) or not decoded or not isinstance(decoded[0], str):
        return None
    payload = decoded[1] if len(decoded) > 1 else None
    return decoded[0], payload


def _value_string(value) -> str:
    if value is None:
        return ""
    return str(value).strip()


def _stable_message_id(raw: dict, recipient_email: str) -> str:
    headers = raw.get("headers") or {}
    for key in ("id", "messageId"):
        val = _value_string(raw.get(key))
        if val:
            return val
    for key in ("message-id", "messageId"):
        val = _value_string(headers.get(key))
        if val:
            return val
    return "\n".join([
        _value_string(headers.get("from")),
        _value_string(headers.get("subject")),
        _value_string(headers.get("date")),
        recipient_email,
    ])


def _flatten_mail(raw: dict, recipient_email: str) -> dict:
    headers = raw.get("headers") or {}
    return {
        "id": _stable_message_id(raw, recipient_email),
        "from": _value_string(headers.get("from")),
        "to": recipient_email,
        "subject": _value_string(headers.get("subject")),
        "text": _value_string(raw.get("text")),
        "html": _value_string(raw.get("html")),
        "date": _value_string(headers.get("date")),
        "isRead": False,
        "attachments": raw.get("attachments") or [],
    }


def generate_email(domain: Optional[str] = None) -> EmailInfo:
    host = _normalize_host(domain)
    ws = _connect_socket(host)
    try:
        _send_event(ws, "request shortid", True)
        while True:
            packet = ws.recv()
            if isinstance(packet, bytes):
                packet = packet.decode("utf-8", errors="ignore")
            if not isinstance(packet, str):
                continue
            if packet == "2":
                ws.send("3")
                continue
            parsed = _parse_event(packet)
            if not parsed:
                continue
            event, payload = parsed
            if event == "shortid" and isinstance(payload, str) and payload.strip():
                return EmailInfo(channel=CHANNEL, email=f"{payload.strip()}@{host}")
    finally:
        try:
            ws.close()
        except Exception:  # noqa: BLE001
            pass


def _ws_run(email: str, local: str, host: str, box: _TempmailCnBox) -> None:
    try:
        ws = _connect_socket(host)
    except Exception:  # noqa: BLE001
        with _lock:
            box.started = False
        return

    try:
        _send_event(ws, "set shortid", local)
        while True:
            packet = ws.recv()
            if isinstance(packet, bytes):
                packet = packet.decode("utf-8", errors="ignore")
            if not isinstance(packet, str):
                continue
            if packet == "2":
                ws.send("3")
                continue
            parsed = _parse_event(packet)
            if not parsed:
                continue
            event, payload = parsed
            if event != "mail" or not isinstance(payload, dict):
                continue
            em = normalize_email(_flatten_mail(payload, email), email)
            if not em.id:
                continue
            with _lock:
                if em.id in box.by_id:
                    continue
                box.by_id[em.id] = em
                box.emails.append(em)
    except Exception:  # noqa: BLE001
        pass
    finally:
        try:
            ws.close()
        except Exception:  # noqa: BLE001
            pass
        with _lock:
            box.started = False


def _ensure_ws(email: str) -> None:
    local, host = _split_email(email)
    box = _get_box(email)
    with _lock:
        if box.started:
            return
        box.started = True
        t = threading.Thread(target=_ws_run, args=(email, local, host, box), daemon=True)
        box.thread = t
        t.start()
    time.sleep(0.08)


def get_emails(email: str) -> List[Email]:
    _ensure_ws(email)
    box = _get_box(email)
    with _lock:
        return list(box.emails)
