"""
mailmomy 域名变体渠道：282mail.com
固定使用域名 282mail.com 生成邮箱，收信复用 mailmomy 的 get_emails。
"""

from typing import List

from ..types import Email, EmailInfo
from . import mailmomy

CHANNEL = "282mail-com"
DOMAIN = "282mail.com"


def generate_email() -> EmailInfo:
    """生成 282mail.com 域名的临时邮箱；Token 即邮箱地址本身"""
    email = f"{mailmomy._random_local(10)}@{DOMAIN}"
    return EmailInfo(channel=CHANNEL, email=email, _token=email)


def get_emails(email: str) -> List[Email]:
    """收信直接委托给 mailmomy（同后端 API）"""
    return mailmomy.get_emails(email)
