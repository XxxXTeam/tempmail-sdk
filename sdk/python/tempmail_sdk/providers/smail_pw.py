import json
import math
import re
from .. import http as tm_http
from ..logger import get_logger
from ..types import EmailInfo
from ..normalize import normalize_email

CHANNEL = "smail-pw"
ROOT_DATA_URL = "https://smail.pw/_root.data"

_HEADERS = {
    "Accept": "*/*",
    "accept-language": "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "cache-control": "no-cache",
    "dnt": "1",
    "origin": "https://smail.pw",
    "pragma": "no-cache",
    "referer": "https://smail.pw/",
    "sec-ch-ua": '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
    "sec-ch-ua-mobile": "?0",
    "sec-ch-ua-platform": '"Windows"',
    "sec-fetch-dest": "empty",
    "sec-fetch-mode": "cors",
    "sec-fetch-site": "same-origin",
}

_RE_QUOTED_INBOX = re.compile(r'"([a-z0-9][a-z0-9.-]*@smail\.pw)"', re.I)
_RE_PLAIN_INBOX = re.compile(r"\b([a-z0-9][a-z0-9.-]*@smail\.pw)\b", re.I)
_RE_MAIL = re.compile(
    r'"id","([^"]+)","to_address","([^"]*)","from_name","([^"]*)","from_address","([^"]*)","subject","([^"]*)","time",(\d+)'
)
_RE_MAIL_NO_SUBJ = re.compile(
    r'"id","([^"]+)","to_address","([^"]*)","from_name","([^"]*)","from_address","([^"]*)","subject","time",(\d+)'
)
_RE_MAIL2 = re.compile(
    r'"id","([^"]+)","from_name","([^"]*)","from_address","([^"]*)","subject","([^"]*)","time",(\d+)'
)


def _walk_plain_row_emails(node, recipient: str, out: list, seen: set) -> None:
    """官方 loader 返回 D1 行 dict：id/to_address/from_name/from_address/subject/time（Flight 里多为 JSON 对象）。"""
    if node is None or not isinstance(node, (dict, list)):
        return
    oid = id(node)
    if oid in seen:
        return
    seen.add(oid)
    if isinstance(node, list):
        for el in node:
            _walk_plain_row_emails(el, recipient, out, seen)
        return
    subj = node.get("subject")
    if isinstance(subj, str):
        tr = node.get("time")
        if isinstance(tr, int):
            time_ms = int(tr)
        elif isinstance(tr, float) and not math.isnan(tr):
            time_ms = int(tr)
        elif isinstance(tr, str) and tr.isdigit():
            time_ms = int(tr)
        else:
            time_ms = None
        if time_ms is not None:
            out.append(
                {
                    "id": str(node.get("id") or ""),
                    "to_address": str(node.get("to_address") or recipient),
                    "from_name": str(node.get("from_name") or ""),
                    "from_address": str(node.get("from_address") or ""),
                    "subject": subj,
                    "date": time_ms,
                    "text": "",
                    "html": "",
                    "attachments": [],
                }
            )
    for v in node.values():
        if isinstance(v, (dict, list)):
            _walk_plain_row_emails(v, recipient, out, seen)


def _session_cookie(resp) -> str:
    v = resp.cookies.get("__session")
    if v:
        return f"__session={v}"
    sc = resp.headers.get("Set-Cookie") or ""
    m = re.search(r"__session=([^;]+)", sc)
    return f"__session={m.group(1)}" if m else ""


def _extract_inbox(text: str) -> str:
    m = _RE_QUOTED_INBOX.search(text)
    if m:
        return m.group(1)
    m = _RE_PLAIN_INBOX.search(text)
    return m.group(1) if m else ""


def _is_flight_deferred_row(obj) -> bool:
    return isinstance(obj, dict) and obj and all(str(k).startswith("_") for k in obj)


def _resolve_flight_deferred_row(root: list, obj: dict) -> dict | None:
    fields: dict = {}
    for k, v in obj.items():
        if not str(k).startswith("_"):
            continue
        try:
            key_idx = int(str(k)[1:])
            val_idx = int(v)
        except (TypeError, ValueError):
            continue
        if key_idx < 0 or key_idx >= len(root) or val_idx < 0 or val_idx >= len(root):
            continue
        key = root[key_idx]
        val = root[val_idx]
        if key == "time" and (isinstance(val, int) or (isinstance(val, str) and str(val).isdigit())):
            fields["time"] = int(val)
        elif isinstance(key, str) and isinstance(val, str):
            fields[key] = val
    if not fields.get("id") and "time" not in fields:
        return None
    return fields


def _row_fields_to_mail(fields: dict, recipient: str) -> dict | None:
    time_ms = fields.get("time")
    if not isinstance(time_ms, int):
        return None
    to_addr = str(fields.get("to_address") or recipient)
    subject = str(fields.get("subject") or "")
    if subject == to_addr or subject.endswith("@smail.pw"):
        subject = ""
    return {
        "id": str(fields.get("id") or ""),
        "to_address": to_addr,
        "from_name": str(fields.get("from_name") or ""),
        "from_address": str(fields.get("from_address") or ""),
        "subject": subject,
        "date": time_ms,
        "text": "",
        "html": "",
        "attachments": [],
    }


def _parse_flight_root(root: list, recipient: str) -> list:
    mails = []
    for i, el in enumerate(root):
        if el != "emails" or i + 1 >= len(root):
            continue
        refs = root[i + 1]
        if not isinstance(refs, list):
            break
        for r in refs:
            idx = int(r) if isinstance(r, int) else None
            if idx is None and isinstance(r, str) and r.isdigit():
                idx = int(r)
            if idx is None or idx < 0 or idx >= len(root):
                continue
            node = root[idx]
            if _is_flight_deferred_row(node):
                fields = _resolve_flight_deferred_row(root, node)
                if fields:
                    mail = _row_fields_to_mail(fields, recipient)
                    if mail:
                        mails.append(mail)
        break
    return mails


def _fetch_email_body(token: str, mail_id: str) -> dict:
    if not mail_id or mail_id.startswith("__smail_"):
        return {"text": "", "html": ""}
    try:
        resp = tm_http.get(
            f"https://smail.pw/api/email/{mail_id}",
            headers={**_HEADERS, "Cookie": token, "Accept": "application/json"},
        )
        if resp.status_code != 200:
            return {"text": "", "html": ""}
        data = resp.json()
        html = data.get("body") if isinstance(data, dict) else ""
        return {"text": "", "html": html if isinstance(html, str) else ""}
    except Exception:
        return {"text": "", "html": ""}


def _merge_by_id(rows: list) -> list:
    by_id = {}
    anon = 0
    for mail in rows:
        mid = str(mail.get("id") or "")
        if not mid:
            mid = f"__smail_{anon}_{mail.get('date', '')}_{str(mail.get('subject', ''))[:48]}"
            mail["id"] = mid
            anon += 1
        if mid not in by_id:
            by_id[mid] = mail
    return list(by_id.values())


def _parse_mails(text: str, recipient: str) -> list:
    chunks: list = []
    reg = []
    for m in _RE_MAIL.finditer(text):
        reg.append(
            {
                "id": m.group(1),
                "to_address": m.group(2) or recipient,
                "from_name": m.group(3),
                "from_address": m.group(4),
                "subject": m.group(5),
                "date": int(m.group(6)),
                "text": "",
                "html": "",
                "attachments": [],
            }
        )
    if reg:
        chunks.append(reg)
    reg0 = []
    for m in _RE_MAIL_NO_SUBJ.finditer(text):
        reg0.append(
            {
                "id": m.group(1),
                "to_address": m.group(2) or recipient,
                "from_name": m.group(3),
                "from_address": m.group(4),
                "subject": "",
                "date": int(m.group(5)),
                "text": "",
                "html": "",
                "attachments": [],
            }
        )
    if reg0:
        chunks.append(reg0)
    reg2 = []
    for m in _RE_MAIL2.finditer(text):
        reg2.append(
            {
                "id": m.group(1),
                "to_address": recipient,
                "from_name": m.group(2),
                "from_address": m.group(3),
                "subject": m.group(4),
                "date": int(m.group(5)),
                "text": "",
                "html": "",
                "attachments": [],
            }
        )
    if reg2:
        chunks.append(reg2)
    try:
        root = json.loads(text)
        plain = []
        _walk_plain_row_emails(root, recipient, plain, set())
        if plain:
            chunks.append(plain)
        if isinstance(root, list):
            flight = _parse_flight_root(root, recipient)
            if flight:
                chunks.append(flight)
    except Exception:
        get_logger().debug("smail-pw: json parse in _parse_mails failed", exc_info=True)
    flat = [row for part in chunks for row in part]
    return _merge_by_id(flat)


def generate_email(**kwargs) -> EmailInfo:
    resp = tm_http.post(
        ROOT_DATA_URL,
        data="intent=generate",
        headers={
            **_HEADERS,
            "Content-Type": "application/x-www-form-urlencoded;charset=UTF-8",
        },
    )
    resp.raise_for_status()
    cookie = _session_cookie(resp)
    if not cookie:
        raise Exception("Failed to extract __session cookie")
    text = resp.text
    email = _extract_inbox(text)
    if not email:
        raise Exception("Failed to parse inbox from smail.pw response")
    return EmailInfo(channel=CHANNEL, email=email, _token=cookie)


def get_emails(token: str, email: str = "", **kwargs) -> list:
    resp = tm_http.get(
        ROOT_DATA_URL,
        headers={**_HEADERS, "Cookie": token},
    )
    resp.raise_for_status()
    raw_list = _parse_mails(resp.text, email)
    out = [normalize_email(r, email) for r in raw_list]
    for item in out:
        if not item.id:
            continue
        body = _fetch_email_body(token, item.id)
        if body.get("html"):
            item.html = body["html"]
        if body.get("text"):
            item.text = body["text"]
    return out
