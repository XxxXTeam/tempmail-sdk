from __future__ import annotations

import hashlib
import json
import os
import random
from typing import Any, Dict, List, Optional

_UA_POOL: List[str] = [
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/121.0.0.0 Safari/537.36 Edg/121.0.0.0",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/119.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:122.0) Gecko/20100101 Firefox/122.0",
]
_PLATFORM = ["Win32", "MacIntel", "Linux x86_64"]
_LANG = ["zh-CN", "en-US", "en-GB", "ja-JP"]
_LANGS = ["zh-CN,zh,en", "en-US,en", "ja-JP,ja,en"]
_TZ = ["Asia/Shanghai", "America/New_York", "Europe/London", "UTC"]
_MEM = [4, 8, 16]
_DEPTH = [24, 30]
_RATIO = [1, 1.25, 1.5, 2]
_W = [1920, 2560, 1680, 1536, 1366]
_H = [1080, 1440, 1050, 864, 768]


def _random_browser_like_profile() -> Dict[str, Any]:
    w = random.choice(_W) + random.randrange(0, 81)
    h = random.choice(_H) + random.randrange(0, 41)
    avail_h = h - 24 - random.randrange(0, 97)
    if avail_h < 0:
        avail_h = h
    return {
        "userAgent": random.choice(_UA_POOL),
        "platform": random.choice(_PLATFORM),
        "language": random.choice(_LANG),
        "languages": random.choice(_LANGS),
        "hardwareConcurrency": random.randrange(4, 33),
        "deviceMemory": random.choice(_MEM),
        "timezone": random.choice(_TZ),
        "colorDepth": random.choice(_DEPTH),
        "pixelRatio": random.choice(_RATIO),
        "screen": {
            "width": w,
            "height": h,
            "availWidth": w,
            "availHeight": avail_h,
        },
    }


def _profile_json_bytes(profile: Dict[str, Any]) -> bytes:
    """与 TS JSON.stringify(profile, Object.keys(profile).sort()) 一致：仅顶层 key 排序。"""
    keys_sorted = sorted(profile.keys())
    ordered = {k: profile[k] for k in keys_sorted}
    return json.dumps(ordered, separators=(",", ":")).encode("utf-8")


def derive_linshi_api_path_key(visitor_id: str) -> str:
    if len(visitor_id) < 4:
        raise ValueError("visitorId 过短")
    e = visitor_id[:-2]
    t = sum(ord(c) % 5 for c in e)
    if t < 10:
        t = 10
    if t >= 100:
        t = 99
    ts = str(t)
    e = e[:3] + ts[0] + e[3:]
    e = e[:12] + ts[1] + e[12:]
    return e


def synthetic_visitor_id_from_profile(
    profile: Dict[str, Any], salt: Optional[bytes] = None
) -> str:
    if salt is None:
        salt = os.urandom(16)
    payload = _profile_json_bytes(profile)
    h = hashlib.sha256(payload + salt).hexdigest()
    return h[:32]


def random_synthetic_linshi_api_path_key() -> str:
    p = _random_browser_like_profile()
    vid = synthetic_visitor_id_from_profile(p, None)
    return derive_linshi_api_path_key(vid)
