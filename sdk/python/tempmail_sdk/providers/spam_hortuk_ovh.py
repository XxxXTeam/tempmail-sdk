"""
Mailinator 姊妹域名渠道：spam.hortuk.ovh
MX 指向 mail.mailinator.com，收信复用 mailinator 的 domain=public API
"""

from typing import List

from ..types import Email, EmailInfo
from . import mailinator

CHANNEL = "spam-hortuk-ovh"
DOMAIN = "spam.hortuk.ovh"


def generate_email() -> EmailInfo:
    local = mailinator._random_string(12)
    return EmailInfo(channel=CHANNEL, email=f"{local}@{DOMAIN}")


def get_emails(token: str, email: str = "", **kwargs) -> List[Email]:
    return mailinator.get_emails(token, email, **kwargs)
