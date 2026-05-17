"""
Socket.IO 临时邮箱共享实现
用于 mjj.cm、mail.xiuvi.cn、linshi.co 等使用相同 Socket.IO 协议的站点
"""

import json
import threading
import time
from typing import Dict, List, Optional, Tuple

import websocket as ws_client

from ..normalize import normalize_email
from ..types import Email, EmailInfo

CONNECT_TIMEOUT = 15
HANDSHAKE_WAIT = 1.0
INITIAL_SYNC_WAIT = 0.08
SOCKET_IO_VERSIONS = [4, 3]


def _socket_url(host: str, eio: int) -> str:
    return f"wss://{host}/socket.io/?EIO={eio}&transport=websocket"


def _parse_event_packet(packet: str) -> Optional[Tuple[str, any]]:
    """解析 Socket.IO 事件包"""
    if not packet.startswith("42"):
        return None
    try:
        decoded = json.loads(packet[2:])
        if not isinstance(decoded, list) or len(decoded) < 2 or not isinstance(decoded[0], str):
            return None
        return (decoded[0], decoded[1])
    except (json.JSONDecodeError, IndexError):
        return None


def _send_event(ws, event: str, payload) -> None:
    """发送 Socket.IO 事件"""
    data = json.dumps([event, payload])
    ws.send(f"42{data}")


def _flatten_mail(raw: dict, recipient_email: str) -> dict:
    """展平邮件数据"""
    headers = raw.get("headers") or {}

    msg_id = (
        raw.get("id")
        or raw.get("messageId")
        or headers.get("message-id")
        or headers.get("messageId")
        or f"{headers.get('from', raw.get('from', ''))}\n{headers.get('subject', raw.get('subject', ''))}\n{headers.get('date', raw.get('date', ''))}\n{recipient_email}"
    )

    from_addr = headers.get("from") or raw.get("from") or ""
    subject = headers.get("subject") or raw.get("subject") or ""
    text = raw.get("text") or raw.get("body") or ""
    html_content = raw.get("html") or ""
    date = headers.get("date") or raw.get("date") or ""

    return {
        "id": str(msg_id),
        "from": from_addr,
        "to": recipient_email,
        "subject": subject,
        "text": text,
        "html": html_content,
        "date": date,
        "isRead": False,
    }


class _BoxState:
    def __init__(self, email: str, channel: str):
        self.email = email
        self.channel = channel
        self.emails: List[Email] = []
        self.seen_ids: set = set()
        self.ws = None
        self.lock = threading.Lock()


class SocketIoMailProvider:
    """Socket.IO 临时邮箱 provider 工厂"""

    def __init__(self, channel: str, default_host: str):
        self.channel = channel
        self.default_host = default_host
        self._mailboxes: Dict[str, _BoxState] = {}
        self._lock = threading.Lock()

    def _connect_socket(self, host: str):
        """连接 Socket.IO WebSocket"""
        last_err = None
        for version in SOCKET_IO_VERSIONS:
            url = _socket_url(host, version)
            try:
                ws = ws_client.create_connection(
                    url,
                    timeout=CONNECT_TIMEOUT,
                    header={
                        "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
                        "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
                        "Cache-Control": "no-cache",
                        "DNT": "1",
                        "Pragma": "no-cache",
                        "Origin": f"https://{host}",
                    },
                )
                # 等待 Engine.IO open 包并发送 Socket.IO connect
                sent_connect = False
                for _ in range(10):
                    msg = ws.recv()
                    if msg == "2":
                        ws.send("3")
                        continue
                    if not sent_connect:
                        if not msg.startswith("0"):
                            raise RuntimeError(f"{self.channel}: unexpected open packet for EIO={version}")
                        sent_connect = True
                        ws.send("40")
                        time.sleep(HANDSHAKE_WAIT)
                        return ws
                    if msg.startswith("40"):
                        return ws
                ws.close()
            except Exception as e:
                last_err = e
                continue
        raise last_err or RuntimeError(f"{self.channel}: websocket handshake failed")

    def _request_shortid(self, host: str) -> str:
        """请求 shortid"""
        ws = self._connect_socket(host)
        try:
            _send_event(ws, "request shortid", True)
            ws.settimeout(CONNECT_TIMEOUT)
            for _ in range(50):
                msg = ws.recv()
                if msg == "2":
                    ws.send("3")
                    continue
                parsed = _parse_event_packet(msg)
                if parsed and parsed[0] == "shortid" and isinstance(parsed[1], str):
                    return parsed[1]
            raise RuntimeError(f"{self.channel}: 等待 shortid 超时")
        finally:
            ws.close()

    def generate_email(self) -> EmailInfo:
        """创建临时邮箱"""
        host = self.default_host
        shortid = self._request_shortid(host)
        email_addr = f"{shortid}@{host}"
        return EmailInfo(channel=self.channel, email=email_addr)

    def _get_state(self, email: str) -> _BoxState:
        key = email.strip().lower()
        with self._lock:
            if key not in self._mailboxes:
                self._mailboxes[key] = _BoxState(email, self.channel)
            return self._mailboxes[key]

    def _ensure_mailbox(self, email: str) -> None:
        """确保 WebSocket 连接存在"""
        st = self._get_state(email)
        with st.lock:
            if st.ws is not None:
                return

        at_idx = email.find("@")
        if at_idx <= 0:
            raise ValueError(f"{self.channel}: invalid email address")
        local = email[:at_idx]
        host = email[at_idx + 1:] or self.default_host

        ws = self._connect_socket(host)
        _send_event(ws, "set shortid", local)

        with st.lock:
            st.ws = ws

        def _listen():
            try:
                while True:
                    msg = ws.recv()
                    if msg == "2":
                        ws.send("3")
                        continue
                    parsed = _parse_event_packet(msg)
                    if not parsed or parsed[0] != "mail" or not isinstance(parsed[1], dict):
                        continue
                    flat = _flatten_mail(parsed[1], email)
                    normalized = normalize_email(flat, email)
                    with st.lock:
                        if normalized.id and normalized.id not in st.seen_ids:
                            st.seen_ids.add(normalized.id)
                            st.emails.append(normalized)
            except Exception:
                with st.lock:
                    if st.ws is ws:
                        st.ws = None
                try:
                    ws.close()
                except Exception:
                    pass

        t = threading.Thread(target=_listen, daemon=True)
        t.start()
        time.sleep(INITIAL_SYNC_WAIT)

    def get_emails(self, email: str) -> List[Email]:
        """获取邮件列表"""
        self._ensure_mailbox(email)
        st = self._get_state(email)
        with st.lock:
            return list(st.emails)


# 三个渠道实例
_mjj_cm_provider = SocketIoMailProvider("mjj-cm", "mjj.cm")
_mail_xiuvi_provider = SocketIoMailProvider("mail-xiuvi", "mail.xiuvi.cn")
_linshi_co_provider = SocketIoMailProvider("linshi-co", "linshi.co")
