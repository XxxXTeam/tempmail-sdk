"""
Guerrillamail 镜像渠道实现
sharklasers.com / grr.la / guerrillamail.info 共用同一套 API，仅 baseURL 不同
"""

import re
from urllib.parse import quote
from .. import http as tm_http
from ..types import EmailInfo
from ..normalize import normalize_email

DEFAULT_HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
}


def _create_mirror_provider(channel: str, base_url: str):
    """创建镜像渠道的 generate_email 和 get_emails 函数"""

    def generate_email(**kwargs) -> EmailInfo:
        """创建临时邮箱"""
        resp = tm_http.get(
            f"{base_url}?f=get_email_address&lang=en",
            headers=DEFAULT_HEADERS,
        )
        resp.raise_for_status()
        data = resp.json()

        if not data.get("email_addr") or not data.get("sid_token"):
            raise Exception(f"{channel}: missing email_addr or sid_token")

        expires_at = None
        if data.get("email_timestamp"):
            expires_at = (data["email_timestamp"] + 3600) * 1000

        return EmailInfo(
            channel=channel,
            email=data["email_addr"],
            _token=data["sid_token"],
            expires_at=expires_at,
        )

    def get_emails(token: str, email: str = "", **kwargs) -> list:
        """获取邮件列表"""
        resp = tm_http.get(
            f"{base_url}?f=check_email&seq=0&sid_token={quote(token)}",
            headers=DEFAULT_HEADERS,
        )
        resp.raise_for_status()
        data = resp.json()

        mail_list = data.get("list") or []
        if not isinstance(mail_list, list):
            return []

        out = []
        for item in mail_list:
            body = item.get("mail_body") or ""
            if not body and item.get("mail_id"):
                try:
                    dr = tm_http.get(
                        f"{base_url}?f=fetch_email&sid_token={quote(token)}&email_id={quote(str(item['mail_id']))}",
                        headers=DEFAULT_HEADERS,
                    )
                    if dr.ok:
                        detail = dr.json()
                        body = detail.get("mail_body") or ""
                except Exception:  # noqa: BLE001
                    pass
            text = (
                re.sub(r"<[^>]+>", " ", body).strip()
                if body
                else item.get("mail_excerpt", "")
            )
            text = re.sub(r"\s+", " ", text).strip()
            out.append(
                normalize_email(
                    {
                        "id": item.get("mail_id"),
                        "from": item.get("mail_from"),
                        "to": email,
                        "subject": item.get("mail_subject"),
                        "text": text,
                        "html": body,
                        "date": item.get("mail_date", ""),
                        "isRead": item.get("mail_read") == 1,
                    },
                    email,
                )
            )
        return out

    return generate_email, get_emails


# 创建三个镜像渠道
sharklasers_generate, sharklasers_get_emails = _create_mirror_provider(
    "sharklasers", "https://www.sharklasers.com/ajax.php"
)
grrla_generate, grrla_get_emails = _create_mirror_provider(
    "grr-la", "https://www.grr.la/ajax.php"
)
guerrillamail_info_generate, guerrillamail_info_get_emails = _create_mirror_provider(
    "guerrillamail-info", "https://www.guerrillamail.info/ajax.php"
)
spam4me_generate, spam4me_get_emails = _create_mirror_provider(
    "spam4me", "https://www.spam4.me/ajax.php"
)
guerrillamail_com_generate, guerrillamail_com_get_emails = _create_mirror_provider(
    "guerrillamail-com", "https://guerrillamail.com/ajax.php"
)
sharklasers_com_generate, sharklasers_com_get_emails = _create_mirror_provider(
    "sharklasers-com", "https://sharklasers.com/ajax.php"
)
grrla_com_generate, grrla_com_get_emails = _create_mirror_provider(
    "grr-la-com", "https://grr.la/ajax.php"
)
guerrillamail_net_generate, guerrillamail_net_get_emails = _create_mirror_provider(
    "guerrillamail-net", "https://www.guerrillamail.net/ajax.php"
)
guerrillamail_org_generate, guerrillamail_org_get_emails = _create_mirror_provider(
    "guerrillamail-org", "https://www.guerrillamail.org/ajax.php"
)
guerrillamailblock_generate, guerrillamailblock_get_emails = _create_mirror_provider(
    "guerrillamailblock", "https://www.guerrillamailblock.com/ajax.php"
)
guerrillamail_com_www_generate, guerrillamail_com_www_get_emails = (
    _create_mirror_provider(
        "guerrillamail-com-www", "https://www.guerrillamail.com/ajax.php"
    )
)
