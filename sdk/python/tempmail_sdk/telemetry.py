from __future__ import annotations

import json
import platform
import re
import threading
import time
from typing import Any, Dict, List, Optional

import requests

from .config import get_config

_DEFAULT_URL = "https://sdk-1.openel.top/v1/event"
_MAX_BATCH = 32
_FLUSH_SEC = 2.0
_EMAIL_RE = re.compile(r"[^\s@]{1,64}@[^\s@]{1,255}")

_queue: List[Dict[str, Any]] = []
_lock = threading.Lock()
_flush_timer: Optional[threading.Timer] = None
_periodic_started = False
_periodic_lock = threading.Lock()


def _sanitize(msg: str) -> str:
    if not msg:
        return ""
    return _EMAIL_RE.sub("[redacted]", msg)[:400]


def _resolve_url() -> str:
    c = get_config()
    u = (getattr(c, "telemetry_url", None) or "").strip()
    return u or _DEFAULT_URL


def _telemetry_on() -> bool:
    c = get_config()
    v = getattr(c, "telemetry_enabled", None)
    if v is None:
        return True
    return bool(v)


def _sdk_version() -> str:
    try:
        from importlib.metadata import version

        return version("tempemail-sdk")
    except Exception:
        return "0.0.0"


def _post_body(url: str, body: bytes, ver: str) -> None:
    headers = {
        "Content-Type": "application/json; charset=utf-8",
        "User-Agent": f"tempmail-sdk-python/{ver}",
    }
    try:
        requests.post(url, data=body, headers=headers, timeout=8)
    except Exception:
        pass


def _cancel_flush_timer_locked() -> None:
    global _flush_timer
    if _flush_timer is None:
        return
    _flush_timer.cancel()
    _flush_timer = None


def _arm_flush_timer() -> None:
    global _flush_timer
    with _lock:
        _cancel_flush_timer_locked()
        _flush_timer = threading.Timer(_FLUSH_SEC, _flush_batch)
        _flush_timer.daemon = True
        _flush_timer.start()


def _ensure_periodic() -> None:
    global _periodic_started
    with _periodic_lock:
        if _periodic_started:
            return
        _periodic_started = True

        def loop() -> None:
            while True:
                time.sleep(_FLUSH_SEC)
                _flush_batch()

        threading.Thread(target=loop, daemon=True).start()


def _flush_batch() -> None:
    if not _telemetry_on():
        with _lock:
            _queue.clear()
            _cancel_flush_timer_locked()
        return

    with _lock:
        if not _queue:
            return
        events = _queue[:]
        _queue.clear()
        _cancel_flush_timer_locked()

    url = _resolve_url()
    if not url:
        return

    ver = _sdk_version()
    batch: Dict[str, Any] = {
        "schema_version": 2,
        "sdk_language": "python",
        "sdk_version": ver,
        "os": platform.system().lower() or "unknown",
        "arch": platform.machine() or "unknown",
        "events": events,
    }
    body = json.dumps(batch, ensure_ascii=False).encode("utf-8")
    threading.Thread(target=_post_body, args=(url, body, ver), daemon=True).start()


def report_telemetry(
    operation: str,
    channel: str,
    success: bool,
    attempt_count: int,
    channels_tried: int,
    error: str,
) -> None:
    if not _telemetry_on():
        return

    _ensure_periodic()

    ev: Dict[str, Any] = {
        "operation": operation,
        "channel": channel,
        "success": success,
        "attempt_count": attempt_count,
        "ts_ms": int(time.time() * 1000),
    }
    if channels_tried > 0:
        ev["channels_tried"] = channels_tried
    err = _sanitize(error)
    if err:
        ev["error"] = err

    with _lock:
        _queue.append(ev)
        n = len(_queue)

    if n >= _MAX_BATCH:
        _flush_batch()
    else:
        _arm_flush_timer()


def get_sdk_version() -> str:
    """已安装包时从分发元数据读取版本"""
    return _sdk_version()
