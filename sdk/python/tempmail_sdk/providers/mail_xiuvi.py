"""
mail-xiuvi — mail.xiuvi.cn（Socket.IO 临时邮箱）
"""

from typing import List
from ..types import Email, EmailInfo
from .socketio_mail import _mail_xiuvi_provider


def generate_email() -> EmailInfo:
    return _mail_xiuvi_provider.generate_email()


def get_emails(email: str) -> List[Email]:
    return _mail_xiuvi_provider.get_emails(email)
