import { Email, EmailAttachment } from './types';

/**
 * 将各提供商返回的原始邮件数据标准化为统一的 Email 格式
 *
 * 不同渠道的 API 返回字段名各不相同，此函数通过多字段候选策略
 * 将它们统一映射为标准的 Email 结构，保证 SDK 输出一致性。
 *
 * @param raw - 原始邮件数据（来自不同提供商的 API 响应）
 * @param recipientEmail - 收件人邮箱地址，当原始数据中无收件人字段时用作回退值
 * @returns 标准化的 Email 对象
 */
export function normalizeEmail(raw: any, recipientEmail: string = ''): Email {
  let text = normalizeText(raw);
  let html = normalizeHtml(raw);

  /*
   * 修正 text/html 错配：
   * 部分渠道将 HTML 内容放在 body/text 等字段中，
   * 导致 normalizeText 提取到 HTML 内容而 normalizeHtml 为空。
   * 检测 text 中是否包含 HTML 标签，如果是则移动到 html 字段。
   */
  if (text && !html && isHtmlContent(text)) {
    html = text;
    text = '';
  }

  return {
    id: normalizeId(raw),
    from: normalizeFrom(raw),
    to: normalizeTo(raw, recipientEmail),
    subject: normalizeSubject(raw),
    text,
    html,
    date: normalizeDate(raw),
    isRead: normalizeIsRead(raw),
    attachments: normalizeAttachments(raw.attachments),
  };
}

/**
 * 检测内容是否为 HTML
 * 通过检查是否包含常见的 HTML 标签来判断
 */
function isHtmlContent(content: string): boolean {
  const trimmed = content.trim().toLowerCase();
  return trimmed.startsWith('<!doctype html') ||
    trimmed.startsWith('<html') ||
    trimmed.startsWith('<body') ||
    (trimmed.includes('<div') && trimmed.includes('</div>')) ||
    (trimmed.includes('<table') && trimmed.includes('</table>')) ||
    (trimmed.includes('<p') && trimmed.includes('</p>') && trimmed.includes('<'));
}

/**
 * 提取邮件 ID
 * 候选字段: id, eid, _id, mailboxId, messageId, mail_id
 */
function normalizeId(raw: any): string {
  const id = raw.id ?? raw.eid ?? raw._id ?? raw.mailboxId ?? raw.messageId ?? raw.mail_id ?? '';
  return String(id);
}

/**
 * 提取发件人地址
 * 候选字段: from_address, address_from, from, messageFrom, sender
 */
function normalizeFrom(raw: any): string {
  return (
    raw.from_addr ||
    raw.from_address ||
    raw.fromAddress ||
    raw.mail_sender ||
    raw.sender ||
    raw.address_from ||
    raw.from_email ||
    raw.from ||
    raw.messageFrom ||
    ''
  );
}

/**
 * 提取收件人地址，无匹配字段时回退为 recipientEmail
 * 候选字段: to, to_address, name_to, email_address, address
 */
function normalizeTo(raw: any, recipientEmail: string): string {
  return raw.to || raw.to_address || raw.toAddress || raw.name_to || raw.email_address || raw.address || recipientEmail || '';
}

/**
 * 提取邮件主题
 * 候选字段: subject, e_subject
 */
function normalizeSubject(raw: any): string {
  return raw.subject || raw.e_subject || raw.mail_title || '';
}

/**
 * 提取纯文本内容
 * 候选字段: text, body, content, body_text, text_content
 */
function normalizeText(raw: any): string {
  return (
    raw.text ||
    raw.text_body ||
    raw.preview_text ||
    raw.mail_body_text ||
    raw.body ||
    raw.content ||
    raw.body_text ||
    raw.text_content ||
    raw.description ||
    ''
  );
}

/**
 * 提取 HTML 内容
 * 候选字段: html, html_content, body_html
 */
function normalizeHtml(raw: any): string {
  return raw.html || raw.html_body || raw.html_content || raw.body_html || raw.mail_body_html || '';
}

/**
 * 提取并统一日期格式为 ISO 8601
 * 候选字段: received_at, created_at, createdAt, date, timestamp, e_date
 * 其中 timestamp 为 Unix 秒级时间戳，需乘以 1000 转为毫秒
 */
function normalizeDate(raw: any): string {
  try {
    if (raw.received_at) return new Date(raw.received_at).toISOString();
    if (raw.receivedAt) return new Date(String(raw.receivedAt).replace(' ', 'T')).toISOString();
    if (raw.created_at) return new Date(raw.created_at).toISOString();
    if (raw.createdAt) return new Date(raw.createdAt).toISOString();
    if (raw.date) {
      if (typeof raw.date === 'number') return new Date(raw.date).toISOString();
      return new Date(raw.date).toISOString();
    }
    if (raw.timestamp) return new Date(raw.timestamp * 1000).toISOString();
    if (raw.e_date) return new Date(raw.e_date).toISOString();
  } catch {
    /* 日期解析失败，返回空字符串 */
  }
  return '';
}

/**
 * 提取已读状态
 * 候选字段: seen, read, isRead, is_read
 * 支持 boolean / number(0|1) / string('0'|'1') 多种类型
 */
function normalizeIsRead(raw: any): boolean {
  if (typeof raw.seen === 'boolean') return raw.seen;
  if (typeof raw.read === 'boolean') return raw.read;
  if (typeof raw.isRead === 'boolean') return raw.isRead;
  if (typeof raw.is_seen === 'number') return raw.is_seen === 1;
  if (typeof raw.is_seen === 'string') return raw.is_seen === '1';
  if (typeof raw.is_read === 'number') return raw.is_read === 1;
  if (typeof raw.is_read === 'string') return raw.is_read === '1';
  if (typeof raw.is_read === 'boolean') return raw.is_read;
  return false;
}

/**
 * 提取并标准化附件列表
 * 每个附件的字段也采用多候选策略映射
 */
function normalizeAttachments(attachments: any): EmailAttachment[] {
  if (!attachments || !Array.isArray(attachments)) return [];
  return attachments.map((a: any) => ({
    filename: a.filename || a.name || '',
    size: a.size || a.filesize || undefined,
    contentType: a.contentType || a.content_type || a.mimeType || a.mime_type || undefined,
    url: a.url || a.download_url || a.downloadUrl || undefined,
  }));
}
