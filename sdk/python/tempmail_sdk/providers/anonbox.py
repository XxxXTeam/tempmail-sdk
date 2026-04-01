"""
anonbox.net (CCC) — GET /en/ 解析 HTML，HTTP mbox 取信
"""

import re
from email.utils import parsedate_to_datetime
from datetime import datetime, timezone
from .. import http as tm_http
from ..types import EmailInfo
from ..normalize import normalize_email

CHANNEL = "anonbox"
PAGE_URL = "https://anonbox.net/en/"
BASE = "https://anonbox.net"

_MAIL_LINK = re.compile(
    r'<a href="https://anonbox\.net/([^/]+)/([^"]+)">https://anonbox\.net/[^"]+</a>'
)
_DD = re.compile(r"(?is)<dd([^>]*)>([\s\S]*?)</dd>")
_DISPLAY_NONE = re.compile(r"display\s*:\s*none", re.I)
_P = re.compile(r"(?is)<p>([\s\S]*?)</p>")
_TAG = re.compile(r"<[^>]+>")
_EXPIRES = re.compile(
    r"(?is)Your mail address is valid until:</dt>\s*<dd><p>([^<]+)</p>"
)
_MBOX_SPLIT = re.compile(r"\r?\n(?=From )")


def _strip_tags(html: str) -> str:
    s = _TAG.sub("", html)
    return (
        s.replace("&nbsp;", " ")
        .replace("&amp;", "&")
        .replace("&lt;", "<")
        .replace("&gt;", ">")
        .strip()
    )


def _simple_hash(s: str) -> str:
    h = 0
    for c in s:
        h = (h * 31 + ord(c)) & 0xFFFFFFFF
    n = h
    if n == 0:
        return "0"
    digits = "0123456789abcdefghijklmnopqrstuvwxyz"
    out = []
    while n:
        out.append(digits[n % 36])
        n //= 36
    return "".join(reversed(out))


def _parse_en_page(html: str) -> tuple[str, str, str | None]:
    m = _MAIL_LINK.search(html)
    if not m:
        raise RuntimeError("anonbox: mailbox link not found")
    inbox, secret = m.group(1), m.group(2)
    token = f"{inbox}/{secret}"
    address_html = None
    for attrs, inner in _DD.findall(html):
        if _DISPLAY_NONE.search(attrs):
            continue
        pm = _P.search(inner)
        if not pm:
            continue
        p_inner = pm.group(1)
        if "@" not in p_inner:
            continue
        if "googlemail.com" in p_inner.lower():
            continue
        if "anonbox" not in p_inner.lower():
            continue
        address_html = p_inner
        break
    if not address_html:
        raise RuntimeError("anonbox: address paragraph not found")
    merged = _strip_tags(address_html)
    at = merged.find("@")
    if at < 0:
        raise RuntimeError("anonbox: bad address")
    local = merged[:at].strip()
    if not local:
        raise RuntimeError("anonbox: empty local part")
    email = f"{local}@{inbox}.anonbox.net"
    em = _EXPIRES.search(html)
    exp = em.group(1).strip() if em else None
    return email, token, exp


def _decode_qp_if_needed(body: str, header_block: str) -> str:
    te = re.search(r"(?i)^content-transfer-encoding:\s*([^\s]+)", header_block, re.M)
    enc = te.group(1).lower().strip() if te else ""
    if enc != "quoted-printable":
        return body.rstrip("\r\n")
    soft = re.sub(r"=\r?\n", "", body)
    out = bytearray()
    i = 0
    b = soft.encode("latin-1", errors="replace")
    while i < len(b):
        if b[i] == ord("=") and i + 2 < len(b):
            try:
                out.append(int(b[i + 1 : i + 3], 16))
                i += 3
                continue
            except ValueError:
                pass
        out.append(b[i])
        i += 1
    return out.decode("utf-8", errors="replace").rstrip("\r\n")


def _mbox_block_to_raw(block: str, recipient: str) -> dict:
    normalized = block.replace("\r\n", "\n")
    lines = normalized.split("\n")
    i = 0
    if lines and lines[0].startswith("From "):
        i = 1
    headers: dict[str, str] = {}
    cur_key = ""
    while i < len(lines):
        line = lines[i]
        if line == "":
            i += 1
            break
        if line[0] in " \t" and cur_key:
            headers[cur_key] = headers[cur_key] + " " + line.strip()
        else:
            idx = line.find(":")
            if idx > 0:
                cur_key = line[:idx].strip().lower()
                headers[cur_key] = line[idx + 1 :].strip()
        i += 1
    body = "\n".join(lines[i:])
    ct = (headers.get("content-type") or "").lower()
    text = ""
    html = ""
    if "multipart/" in ct:
        bm = re.search(r'(?i)boundary="?([^";\s]+)"?', headers.get("content-type", ""))
        if bm:
            boundary = re.escape(bm.group(1))
            part_re = re.compile(rf"\r?\n--{boundary}(?:--)?\r?\n")
            for part in part_re.split(body):
                part = part.strip()
                if not part or part == "--":
                    continue
                sep = part.find("\n\n")
                if sep < 0:
                    continue
                ph, pb = part[:sep], part[sep + 2 :]
                pct_m = re.search(r"(?i)^content-type:\s*([^\s;]+)", ph, re.M)
                pct = (pct_m.group(1).lower() if pct_m else "") or ""
                if pct == "text/plain":
                    text = _decode_qp_if_needed(pb, ph)
                elif pct == "text/html":
                    html = _decode_qp_if_needed(pb, ph)
    if not text and not html:
        if "text/html" in ct:
            html = _decode_qp_if_needed(body, headers.get("content-type", ""))
        else:
            text = _decode_qp_if_needed(body, headers.get("content-type", ""))
    date_str = headers.get("date") or ""
    if date_str:
        try:
            date_str = parsedate_to_datetime(date_str.strip()).isoformat()
        except (TypeError, ValueError):
            pass
    if not date_str:
        date_str = datetime.now(timezone.utc).isoformat()
    msg_id = headers.get("message-id") or _simple_hash(block[:512])
    return {
        "id": msg_id,
        "from": headers.get("from", ""),
        "to": headers.get("to") or recipient,
        "subject": headers.get("subject", ""),
        "body_text": text,
        "body_html": html,
        "date": date_str,
        "isRead": False,
        "attachments": [],
    }


def generate_email() -> EmailInfo:
    r = tm_http.get(
        PAGE_URL,
        headers={
            "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
            "Accept": "text/html,application/xhtml+xml",
        },
        timeout=15,
    )
    r.raise_for_status()
    email, token, exp = _parse_en_page(r.text)
    return EmailInfo(
        channel=CHANNEL,
        email=email,
        _token=token,
        created_at=exp,
    )


def get_emails(token: str, email: str) -> list:
    if not token:
        raise ValueError("token is required for anonbox channel")
    path = token if token.endswith("/") else token + "/"
    url = f"{BASE}/{path}"
    r = tm_http.get(
        url,
        headers={
            "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
            "Accept": "text/plain,*/*",
        },
        timeout=15,
    )
    if r.status_code == 404:
        return []
    r.raise_for_status()
    raw = r.text.strip()
    if not raw:
        return []
    blocks = [b.strip() for b in _MBOX_SPLIT.split(raw) if b.strip()]
    return [normalize_email(_mbox_block_to_raw(b, email), email) for b in blocks]
