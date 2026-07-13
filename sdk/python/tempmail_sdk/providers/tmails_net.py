from typing import List
from ..types import Email, EmailInfo
from . import mailinator

CHANNEL = "tmails-net"
DOMAIN = "tmails.net"


def generate_email() -> EmailInfo:
    return EmailInfo(channel=CHANNEL, email=f"{mailinator._random_string(12)}@{DOMAIN}")


def get_emails(token: str, email: str = "", **kwargs) -> List[Email]:
    return mailinator.get_emails(token, email, **kwargs)
