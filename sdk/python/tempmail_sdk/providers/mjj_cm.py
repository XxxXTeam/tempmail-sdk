"""
mjj-cm — mjj.cm（Socket.IO 临时邮箱）
"""

from typing import List
from ..types import Email, EmailInfo
from .socketio_mail import _mjj_cm_provider


def generate_email() -> EmailInfo:
    return _mjj_cm_provider.generate_email()


def get_emails(email: str) -> List[Email]:
    return _mjj_cm_provider.get_emails(email)
