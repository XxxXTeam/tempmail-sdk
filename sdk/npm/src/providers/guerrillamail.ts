/**
 * Guerrilla Mail 渠道实现
 * API 文档: https://www.guerrillamail.com/GuerrillaMailAPI.html
 * 
 * 特点:
 * - 无需认证，公开 JSON API
 * - 通过 sid_token 维持会话
 * - 邮箱有效期 60 分钟
 */

import { EmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';

const CHANNEL: Channel = 'guerrillamail';
const BASE_URL = 'https://api.guerrillamail.com/ajax.php';

/**
 * 创建临时邮箱
 * API: GET ajax.php?f=get_email_address
 * 返回 email_addr + sid_token（用于后续获取邮件）
 */
export async function generateEmail(): Promise<EmailInfo> {
  const response = await fetch(`${BASE_URL}?f=get_email_address&lang=en`, {
    method: 'GET',
    headers: {
      'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36',
    },
  });

  if (!response.ok) {
    throw new Error(`Failed to generate email: ${response.status}`);
  }

  const data = await response.json();

  if (!data.email_addr || !data.sid_token) {
    throw new Error('Failed to generate email: missing email_addr or sid_token');
  }

  return {
    channel: CHANNEL,
    email: data.email_addr,
    token: data.sid_token,
    expiresAt: data.email_timestamp ? (data.email_timestamp + 3600) * 1000 : undefined,
  };
}

/**
 * 获取邮件列表
 * API: GET ajax.php?f=check_email&seq=0&sid_token=xxx
 * 返回 list 数组，每个元素包含 mail_id, mail_from, mail_subject, mail_body 等
 */
export async function getEmails(token: string, email: string): Promise<Email[]> {
  const response = await fetch(`${BASE_URL}?f=check_email&seq=0&sid_token=${encodeURIComponent(token)}`, {
    method: 'GET',
    headers: {
      'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36',
    },
  });

  if (!response.ok) {
    throw new Error(`Failed to get emails: ${response.status}`);
  }

  const data = await response.json();
  const list = Array.isArray(data.list) ? data.list : [];

  return list.map((item: any) => normalizeEmail({
    id: item.mail_id,
    from: item.mail_from,
    to: email,
    subject: item.mail_subject,
    text: item.mail_body || item.mail_excerpt || '',
    html: item.mail_body || '',
    date: item.mail_date || '',
    isRead: item.mail_read === 1,
  }, email));
}
