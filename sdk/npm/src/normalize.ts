import { Email, EmailAttachment } from './types';

/**
 * 将各提供商返回的原始邮件数据标准化为统一的 Email 格式
 */
export function normalizeEmail(raw: any, recipientEmail: string = ''): Email {
  return {
    id: normalizeId(raw),
    from: normalizeFrom(raw),
    to: normalizeTo(raw, recipientEmail),
    subject: normalizeSubject(raw),
    text: normalizeText(raw),
    html: normalizeHtml(raw),
    date: normalizeDate(raw),
    isRead: normalizeIsRead(raw),
    attachments: normalizeAttachments(raw.attachments),
  };
}

function normalizeId(raw: any): string {
  const id = raw.id ?? raw.eid ?? raw._id ?? raw.mailboxId ?? raw.messageId ?? raw.mail_id ?? '';
  return String(id);
}

function normalizeFrom(raw: any): string {
  return raw.from_address || raw.address_from || raw.from || raw.messageFrom || raw.sender || '';
}

function normalizeTo(raw: any, recipientEmail: string): string {
  return raw.to || raw.to_address || raw.name_to || raw.email_address || raw.address || recipientEmail || '';
}

function normalizeSubject(raw: any): string {
  return raw.subject || raw.e_subject || '';
}

function normalizeText(raw: any): string {
  return raw.text || raw.body || raw.content || raw.body_text || raw.text_content || '';
}

function normalizeHtml(raw: any): string {
  return raw.html || raw.html_content || raw.body_html || '';
}

function normalizeDate(raw: any): string {
  try {
    if (raw.received_at) return new Date(raw.received_at).toISOString();
    if (raw.created_at) return new Date(raw.created_at).toISOString();
    if (raw.createdAt) return new Date(raw.createdAt).toISOString();
    if (raw.date) {
      if (typeof raw.date === 'number') return new Date(raw.date).toISOString();
      return new Date(raw.date).toISOString();
    }
    if (raw.timestamp) return new Date(raw.timestamp * 1000).toISOString();
    if (raw.e_date) return new Date(raw.e_date).toISOString();
  } catch {
    // 日期解析失败，返回空字符串
  }
  return '';
}

function normalizeIsRead(raw: any): boolean {
  if (typeof raw.seen === 'boolean') return raw.seen;
  if (typeof raw.read === 'boolean') return raw.read;
  if (typeof raw.isRead === 'boolean') return raw.isRead;
  if (typeof raw.is_read === 'number') return raw.is_read === 1;
  if (typeof raw.is_read === 'string') return raw.is_read === '1';
  if (typeof raw.is_read === 'boolean') return raw.is_read;
  return false;
}

function normalizeAttachments(attachments: any): EmailAttachment[] {
  if (!attachments || !Array.isArray(attachments)) return [];
  return attachments.map((a: any) => ({
    filename: a.filename || a.name || '',
    size: a.size || a.filesize || undefined,
    contentType: a.contentType || a.content_type || a.mimeType || a.mime_type || undefined,
    url: a.url || a.download_url || a.downloadUrl || undefined,
  }));
}
