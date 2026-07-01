/**
 * MailTemp.cc 临时邮箱渠道
 * 网站: mailtemp.cc
 * 域名: @neocea.com
 * PHP POST form-urlencoded API
 */
import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'mailtemp-cc';
const API_URL = 'https://mailtemp.cc/api.php';
const DOMAIN = 'neocea.com';

/** 请求头 */
const HEADERS: Record<string, string> = {
  'Content-Type': 'application/x-www-form-urlencoded',
  'Accept': '*/*',
  'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36',
  'Referer': 'https://mailtemp.cc/',
  'Origin': 'https://mailtemp.cc',
};

/**
 * 创建 MailTemp.cc 临时邮箱
 *
 * 流程:
 * 1. POST api.php，body: action=inbox
 * 2. 返回 JSON 字符串格式的用户名（带引号），需 JSON.parse 去除引号
 * 3. 完整邮箱: {username}@neocea.com
 * 4. token 存储用户名部分
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const res = await fetchWithTimeout(API_URL, {
    method: 'POST',
    headers: HEADERS,
    body: 'action=inbox',
  });
  if (!res.ok) {
    throw new Error(`mailtemp-cc: 创建邮箱失败 HTTP ${res.status}`);
  }

  const raw = await res.text();
  if (!raw?.trim()) {
    throw new Error('mailtemp-cc: 创建邮箱返回空数据');
  }

  /* API 返回 JSON 字符串格式（带引号），如 "vindictiverate"，需要 JSON.parse 去除引号 */
  let username: string;
  try {
    username = JSON.parse(raw.trim());
  } catch {
    /* 如果 JSON.parse 失败，尝试直接使用原始文本（去除首尾引号） */
    username = raw.trim().replace(/^"|"$/g, '');
  }

  if (!username || typeof username !== 'string') {
    throw new Error('mailtemp-cc: 创建邮箱返回无效用户名');
  }

  const email = `${username}@${DOMAIN}`;
  return {
    channel: CHANNEL,
    email,
    token: username,
  };
}

/**
 * 获取 MailTemp.cc 邮箱的邮件列表
 *
 * 流程:
 * 1. POST api.php，body: action=fetch&inbox={username}&last_id=0
 * 2. 返回 JSON 数组，每个元素包含邮件摘要信息
 * 3. 对每封邮件请求详情（获取 body_html）
 * 4. 标准化邮件数据
 */
export async function getEmails(token: string, email: string): Promise<Email[]> {
  if (!email?.trim()) {
    throw new Error('mailtemp-cc: 邮箱地址为空');
  }
  if (!token?.trim()) {
    throw new Error('mailtemp-cc: token 为空');
  }

  const username = token.trim();

  /* 获取邮件列表 */
  const res = await fetchWithTimeout(API_URL, {
    method: 'POST',
    headers: HEADERS,
    body: `action=fetch&inbox=${encodeURIComponent(username)}&last_id=0`,
  });
  if (!res.ok) {
    throw new Error(`mailtemp-cc: 获取邮件失败 HTTP ${res.status}`);
  }

  const list = await res.json() as any[];
  if (!Array.isArray(list) || list.length === 0) {
    return [];
  }

  /* 获取每封邮件的详情（包含 body_html） */
  const emails: Email[] = [];
  for (const item of list) {
    const id = item.id;
    if (!id) continue;

    const detailRes = await fetchWithTimeout(API_URL, {
      method: 'POST',
      headers: HEADERS,
      body: `action=view&id=${encodeURIComponent(id)}&inbox=${encodeURIComponent(username)}`,
    });
    if (!detailRes.ok) continue;

    const detail = await detailRes.json() as Record<string, unknown>;
    emails.push(normalizeEmail(detail, email));
  }

  return emails;
}
