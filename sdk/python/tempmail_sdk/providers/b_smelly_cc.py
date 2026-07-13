"""
Mailinator 姊妹域名渠道：b.smelly.cc
MX 指向 mailinator，收信复用 mailinator 的 domain=public API。
"""

from typing import List

from ..types import Email, EmailInfo
from . import mailinator

CHANNEL = "b-smelly-cc"
DOMAIN = "b.smelly.cc"


def generate_email() -> EmailInfo:
    """生成 b.smelly.cc 域名的临时邮箱"""
    local = mailinator._random_string(12)
    return EmailInfo(channel=CHANNEL, email=f"{local}@{DOMAIN}")


def get_emails(token: str, email: str = "", **kwargs) -> List[Email]:
    """收信直接委托给 mailinator（domain=public 可读所有姊妹域名邮件）"""
    return mailinator.get_emails(token, email, **kwargs)
