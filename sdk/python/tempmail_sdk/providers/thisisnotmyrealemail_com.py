"""
Mailinator 姊妹域名渠道：thisisnotmyrealemail.com
MX 指向 mail.mailinator.com，收信复用 mailinator 的 domain=public API
"""

from typing import List

from ..types import Email, EmailInfo
from . import mailinator

CHANNEL = "thisisnotmyrealemail-com"
DOMAIN = "thisisnotmyrealemail.com"


def generate_email() -> EmailInfo:
    """生成 thisisnotmyrealemail.com 域名的临时邮箱"""
    local = mailinator._random_string(12)
    return EmailInfo(channel=CHANNEL, email=f"{local}@{DOMAIN}")


def get_emails(token: str, email: str = "", **kwargs) -> List[Email]:
    """收信直接委托给 mailinator（domain=public 可读所有姊妹域名邮件）"""
    return mailinator.get_emails(token, email, **kwargs)
