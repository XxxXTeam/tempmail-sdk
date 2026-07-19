import time
from typing import Dict, Optional

BACKEND_GROUPS: Dict[str, list] = {
    "mailinator": [
        "mailinator", "sogetthis-com", "bobmail-info", "suremail-info",
        "binkmail-com", "veryrealemail-com", "chammy-info",
        "thisisnotmyrealemail-com", "notmailinator-com",
        "spamhereplease-com", "sendspamhere-com", "sendfree-org",
    ],
    "getnada": ["getnada", "getnada-com", "getnada-email", "getnada-net"],
    "guerrillamail": ["guerrillamail", "sharklasers"],
    "moakt": [
        "moakt", "drmail-in", "teml-net", "tmpeml-com", "tmpbox-net",
        "moakt-cc", "disbox-net", "tmpmail-org", "tmpmail-net",
        "tmails-net", "disbox-org", "moakt-co", "moakt-ws",
        "tmail-ws", "bareed-ws",
    ],
    "catchmail": ["catchmail", "catchmail-mailistry", "catchmail-zeppost"],
    "mailforspam": [
        "mailforspam", "mailforspam-tempmail-io", "mailforspam-disposable",
    ],
    "tempmail-plus": [
        "tempmail-plus", "fexpost-com", "fexbox-org", "mailbox-in-ua",
        "rover-info", "chitthi-in", "fextemp-com", "any-pink", "merepost-com",
    ],
    "mail-cx": ["mail-cx", "ddker-com"],
    "fake-legal": ["fake-legal", "imgui-de", "pulsewebmenu-de"],
    "neighbours": ["neighbours-sh", "neighbours"],
}

_channel_to_backend: Dict[str, str] = {}
for _backend, _channels in BACKEND_GROUPS.items():
    for _ch in _channels:
        _channel_to_backend[_ch] = _backend


def get_backend(channel: str) -> Optional[str]:
    return _channel_to_backend.get(channel)


_DEFAULT_COOLDOWN = 60.0
_MAX_COOLDOWN = 300.0

_circuit_state: Dict[str, dict] = {}


def is_backend_open(backend: str) -> bool:
    state = _circuit_state.get(backend)
    if state is None:
        return True
    fail_count = state["fail_count"]
    cooldown = min(_DEFAULT_COOLDOWN * (2 ** (fail_count - 1)), _MAX_COOLDOWN)
    if time.time() - state["last_failure"] >= cooldown:
        _circuit_state.pop(backend, None)
        return True
    return False


def record_backend_failure(backend: str) -> None:
    state = _circuit_state.get(backend)
    if state is not None:
        state["last_failure"] = time.time()
        state["fail_count"] += 1
    else:
        _circuit_state[backend] = {"last_failure": time.time(), "fail_count": 1}


def record_backend_success(backend: str) -> None:
    _circuit_state.pop(backend, None)
