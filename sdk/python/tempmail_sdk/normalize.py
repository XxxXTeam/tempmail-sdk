"""
邮件数据标准化模块
将各提供商返回的原始邮件数据标准化为统一的 Email 格式
"""

from datetime import datetime, timezone
from typing import Any, Dict, List, Optional
from .types import Email, EmailAttachment


def normalize_email(raw: Dict[str, Any], recipient_email: str = "") -> Email:
    """
    将各提供商返回的原始邮件数据标准化为统一的 Email 格式

    不同渠道的 API 返回字段名各不相同，此函数通过多字段候选策略
    将它们统一映射为标准的 Email 结构，保证 SDK 输出一致性。
    """
    return Email(
        id=_normalize_id(raw),
        from_addr=_normalize_from(raw),
        to=_normalize_to(raw, recipient_email),
        subject=_normalize_subject(raw),
        text=_normalize_text(raw),
        html=_normalize_html(raw),
        date=_normalize_date(raw),
        is_read=_normalize_is_read(raw),
        attachments=_normalize_attachments(raw.get("attachments")),
    )


def _get_str(raw: Dict[str, Any], *keys: str) -> str:
    """从字典中按优先级顺序提取字符串值"""
    for key in keys:
        val = raw.get(key)
        if val is not None:
            return str(val)
    return ""


def _normalize_id(raw: Dict[str, Any]) -> str:
    """提取邮件 ID，候选字段: id, eid, _id, mailboxId, messageId, mail_id"""
    return _get_str(raw, "id", "eid", "_id", "mailboxId", "messageId", "mail_id")


def _normalize_from(raw: Dict[str, Any]) -> str:
    """提取发件人地址"""
    return _get_str(raw, "from_address", "address_from", "from", "messageFrom", "sender")


def _normalize_to(raw: Dict[str, Any], recipient_email: str) -> str:
    """提取收件人地址，无匹配字段时回退为 recipient_email"""
    result = _get_str(raw, "to", "to_address", "name_to", "email_address", "address")
    return result or recipient_email


def _normalize_subject(raw: Dict[str, Any]) -> str:
    """提取邮件主题"""
    return _get_str(raw, "subject", "e_subject")


def _normalize_text(raw: Dict[str, Any]) -> str:
    """提取纯文本内容"""
    return _get_str(raw, "text", "body", "content", "body_text", "text_content")


def _normalize_html(raw: Dict[str, Any]) -> str:
    """提取 HTML 内容"""
    return _get_str(raw, "html", "html_content", "body_html")


def _normalize_date(raw: Dict[str, Any]) -> str:
    """
    提取并统一日期格式为 ISO 8601
    候选字段: received_at, created_at, createdAt, date, timestamp, e_date
    支持字符串日期、秒级时间戳、毫秒级时间戳多种格式
    """
    for key in ("received_at", "created_at", "createdAt", "date"):
        val = raw.get(key)
        if val is not None:
            if isinstance(val, str) and val:
                try:
                    return datetime.fromisoformat(val.replace("Z", "+00:00")).isoformat()
                except (ValueError, TypeError):
                    return val
            elif isinstance(val, (int, float)) and val > 0:
                try:
                    if val > 1e12:
                        return datetime.fromtimestamp(val / 1000, tz=timezone.utc).isoformat()
                    return datetime.fromtimestamp(val, tz=timezone.utc).isoformat()
                except (ValueError, OSError):
                    pass

    for key in ("timestamp", "e_date"):
        val = raw.get(key)
        if val is not None and isinstance(val, (int, float)) and val > 0:
            try:
                if key == "timestamp" and val < 1e12:
                    return datetime.fromtimestamp(val, tz=timezone.utc).isoformat()
                return datetime.fromtimestamp(val / 1000, tz=timezone.utc).isoformat()
            except (ValueError, OSError):
                pass

    return ""


def _normalize_is_read(raw: Dict[str, Any]) -> bool:
    """
    提取已读状态
    候选字段: seen, read, isRead, is_read
    支持 bool / int(0|1) / str('0'|'1') 多种类型
    """
    for key in ("seen", "read", "isRead"):
        val = raw.get(key)
        if isinstance(val, bool):
            return val

    val = raw.get("is_read")
    if val is not None:
        if isinstance(val, bool):
            return val
        if isinstance(val, (int, float)):
            return int(val) != 0
        if isinstance(val, str):
            return val == "1"

    return False


def _normalize_attachments(attachments: Any) -> List[EmailAttachment]:
    """提取并标准化附件列表"""
    if not attachments or not isinstance(attachments, list):
        return []

    result = []
    for a in attachments:
        if not isinstance(a, dict):
            continue
        result.append(EmailAttachment(
            filename=a.get("filename") or a.get("name") or "",
            size=a.get("size") or a.get("filesize"),
            content_type=a.get("contentType") or a.get("content_type") or a.get("mimeType") or a.get("mime_type"),
            url=a.get("url") or a.get("download_url") or a.get("downloadUrl"),
        ))
    return result
