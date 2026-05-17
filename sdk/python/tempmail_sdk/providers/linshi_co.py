"""
linshi-co — linshi.co（Socket.IO 临时邮箱）
"""

from typing import List
from ..types import Email, EmailInfo
from .socketio_mail import _linshi_co_provider


def generate_email() -> EmailInfo:
    return _linshi_co_provider.generate_email()


def get_emails(email: str) -> List[Email]:
    return _linshi_co_provider.get_emails(email)
