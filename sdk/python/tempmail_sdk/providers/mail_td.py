"""
Mail.TD 渠道 — https://mail.td
使用 SHA-256 Proof-of-Work（工作量证明）创建账户的临时邮箱服务。

获取域名: GET /domains → {"domains":[{"domain":"sugtbt.com","pro_only":false}, ...]}
创建账户: POST /accounts body: {"address","auth_key","pow":{"t":ts,"n":nonce,"d":difficulty}}
          成功 201 → {"id":"uuid","address":"email","token":"jwt"}
          需重试   → {"status":"retry","required_difficulty":20,"token":"retry_token"}
获取邮件: GET /accounts/{id}/messages?page=1 头部 Authorization: Bearer {jwt}
"""

import hashlib
import json
import random
import string
import time
from typing import List

from .. import http as tm_http
from ..normalize import normalize_email
from ..types import Email, EmailInfo

CHANNEL = "mail-td"
BASE_URL = "https://api.mail.td/api"

# 初始 PoW 难度（前导零位数）
_INITIAL_DIFFICULTY = 15
# PoW 难度上限，避免服务端要求过高难度导致本地长时间计算
_MAX_DIFFICULTY = 24
# 创建账户时因 PoW 难度提升而重试的最大次数
_MAX_RETRIES = 5

# 公共请求头
_HEADERS = {
    "Content-Type": "application/json",
    "Accept": "application/json",
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
}


def _random_name(length: int = 12) -> str:
    """生成随机用户名，仅包含小写字母和数字"""
    return "".join(random.choices(string.ascii_lowercase + string.digits, k=length))


def _random_password(length: int = 16) -> str:
    """生成随机密码，用于派生账户 auth_key"""
    return "".join(random.choices(string.ascii_letters + string.digits, k=length))


def _leading_zero_bits(digest: bytes) -> int:
    """计算 digest（字节串）二进制表示的前导零位数"""
    bits = 0
    for byte in digest:
        if byte == 0:
            bits += 8
            continue
        # 找到该字节内最高位的 1，之前的位均为前导零
        for i in range(7, -1, -1):
            if (byte >> i) & 1:
                return bits
            bits += 1
        return bits
    return bits


def _solve_pow(address: str, timestamp: int, difficulty: int) -> str:
    """
    求解 Proof-of-Work，返回满足难度要求的 nonce 字符串。

    输入 = address.lower().strip() + str(timestamp) + str(nonce)
    对输入做 SHA-256，要求哈希结果前导零位数 >= difficulty。
    """
    prefix = address.lower().strip() + str(timestamp)
    nonce = 0
    while True:
        candidate = prefix + str(nonce)
        digest = hashlib.sha256(candidate.encode()).digest()
        if _leading_zero_bits(digest) >= difficulty:
            return str(nonce)
        nonce += 1


def _fetch_domains() -> List[str]:
    """获取可用（非 pro_only）域名列表"""
    resp = tm_http.get(f"{BASE_URL}/domains", headers=_HEADERS)
    resp.raise_for_status()

    data = resp.json()
    if not isinstance(data, dict):
        raise RuntimeError("mail-td: 域名响应格式无效")

    domains = data.get("domains")
    if not isinstance(domains, list):
        raise RuntimeError("mail-td: 未获取到域名列表")

    result: List[str] = []
    for item in domains:
        if not isinstance(item, dict):
            continue
        if item.get("pro_only"):
            continue
        domain = item.get("domain", "")
        if domain:
            result.append(domain)

    if not result:
        raise RuntimeError("mail-td: 无可用域名")
    return result


def generate_email(channel: str = CHANNEL) -> EmailInfo:
    """
    创建 mail.td 临时邮箱

    流程:
    1. GET /domains 获取可用域名并随机选取
    2. 生成随机地址与密码，auth_key = sha256hex(password)
    3. 求解 PoW（初始难度 15），POST /accounts 创建账户
    4. 若服务端返回 status=retry，则按 required_difficulty 重新求解并重试
    5. 成功后 token 存储为 JSON 字符串 {"jwt":...,"id":...}
    """
    domains = _fetch_domains()
    domain = random.choice(domains)
    address = f"{_random_name()}@{domain}"

    password = _random_password()
    auth_key = hashlib.sha256(password.encode()).hexdigest()

    difficulty = _INITIAL_DIFFICULTY

    for _ in range(_MAX_RETRIES + 1):
        if difficulty > _MAX_DIFFICULTY:
            raise RuntimeError(
                f"mail-td: PoW 难度 {difficulty} 超出上限 {_MAX_DIFFICULTY}"
            )

        timestamp = int(time.time())
        nonce = _solve_pow(address, timestamp, difficulty)

        resp = tm_http.post(
            f"{BASE_URL}/accounts",
            headers=_HEADERS,
            json={
                "address": address,
                "auth_key": auth_key,
                "pow": {"t": timestamp, "n": nonce, "d": difficulty},
            },
        )

        data = resp.json()
        if not isinstance(data, dict):
            raise RuntimeError("mail-td: 创建账户响应格式无效")

        # 服务端要求提升难度重试
        if data.get("status") == "retry":
            required = data.get("required_difficulty")
            if not isinstance(required, int) or required <= difficulty:
                difficulty += 1
            else:
                difficulty = required
            continue

        resp.raise_for_status()

        account_id = data.get("id", "")
        jwt = data.get("token", "")
        addr = data.get("address", address)

        if not account_id or not jwt or "@" not in addr:
            raise RuntimeError(f"mail-td: 创建账户失败，响应: {data!r}")

        token = json.dumps({"jwt": jwt, "id": account_id})

        return EmailInfo(
            channel=channel,
            email=addr,
            _token=token,
        )

    raise RuntimeError("mail-td: PoW 重试次数已用尽，创建账户失败")


def get_emails(token: str, email: str) -> List[Email]:
    """
    获取 mail.td 邮件列表

    流程:
    1. 从 token（JSON 字符串）解析出 jwt 与账户 id
    2. GET /accounts/{id}/messages?page=1 携带 Authorization: Bearer {jwt}
    3. 响应: {"messages":[{"id","from":{"address","name"},"subject","text","html","created_at"}]}
    4. 遍历 messages 数组，使用 normalize_email 标准化
    """
    if not token:
        raise ValueError("mail-td: token 不能为空")

    addr = (email or "").strip()
    if not addr:
        raise ValueError("mail-td: 邮箱地址不能为空")

    try:
        info = json.loads(token)
    except (ValueError, TypeError):
        raise ValueError("mail-td: token 格式无效")

    if not isinstance(info, dict):
        raise ValueError("mail-td: token 格式无效")

    jwt = info.get("jwt", "")
    account_id = info.get("id", "")
    if not jwt or not account_id:
        raise ValueError("mail-td: token 缺少 jwt 或 id")

    headers = dict(_HEADERS)
    headers["Authorization"] = f"Bearer {jwt}"

    resp = tm_http.get(
        f"{BASE_URL}/accounts/{account_id}/messages",
        headers=headers,
        params={"page": 1},
    )
    resp.raise_for_status()

    data = resp.json()
    if not isinstance(data, dict):
        return []

    messages = data.get("messages")
    if not isinstance(messages, list):
        return []

    out: List[Email] = []
    for item in messages:
        if not isinstance(item, dict):
            continue

        # 发件人字段为对象 {"address","name"}，提取地址字符串
        sender = item.get("from", {})
        if isinstance(sender, dict):
            from_addr = sender.get("address", "")
        else:
            from_addr = str(sender)

        raw = {
            "id": item.get("id", ""),
            "from": from_addr,
            "to": item.get("to", addr),
            "subject": item.get("subject", ""),
            "text": item.get("text", ""),
            "html": item.get("html", ""),
            "date": item.get("created_at", ""),
        }
        out.append(normalize_email(raw, addr))

    return out
