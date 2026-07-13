"""Mailinator 姊妹域名 jama.trenet.eu"""

from typing import List
from ..types import Email, EmailInfo
from . import mailinator

CHANNEL = "jama-trenet-eu"
DOMAIN = "jama.trenet.eu"


def generate_email() -> EmailInfo:
    local = mailinator._random_string(12)
    return EmailInfo(channel=CHANNEL, email=f"{local}@{DOMAIN}")


def get_emails(token: str, email: str = "", **kwargs) -> List[Email]:
    return mailinator.get_emails(token, email, **kwargs)
