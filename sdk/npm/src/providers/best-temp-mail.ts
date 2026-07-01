/**
 * BestTempMail 临时邮箱渠道
 * 网站: best-temp-mail.com
 * 域名: 由 API 返回（如 @aabkmail.com），不固定
 * 纯 JSON REST API，无需认证，无需 Session
 */
import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'best-temp-mail';
const BASE = 'https://best-temp-mail.com/api/v3';

/** 请求头 */
const HEADERS: Record<string, string> = {
  'Content-Type': 'application/json',
  'Accept': 'application/json',
  'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36',
  'Referer': 'https://best-temp-mail.com/',
  'Origin': 'https://best-temp-mail.com',
};

/**
 * Token 内部结构
 * 存储创建邮箱时所需的 intToken、邮箱 id 和 update_tag
 */
interface BestTempMailToken {
  intToken: string;
  id: string;
  update_tag: string;
}

/**
 * 生成 UUID v4
 * 优先使用 crypto.randomUUID()，不可用时手动生成
 */
function generateUUID(): string {
  if (typeof crypto !== 'undefined' && typeof crypto.randomUUID === 'function') {
    return crypto.randomUUID();
  }
  /* 手动生成 UUID v4 */
  return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, (c) => {
    const r = (Math.random() * 16) | 0;
    const v = c === 'x' ? r : (r & 0x3) | 0x8;
    return v.toString(16);
  });
}

/**
 * 创建 BestTempMail 临时邮箱
 *
 * 流程:
 * 1. 客户端生成 UUID 作为 intToken
 * 2. POST /createEmail 创建邮箱
 * 3. 将 intToken、id、update_tag 序列化为 JSON 存入 token
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const intToken = generateUUID();

  const res = await fetchWithTimeout(`${BASE}/createEmail`, {
    method: 'POST',
    headers: HEADERS,
    body: JSON.stringify({ intToken }),
  });
  if (!res.ok) {
    throw new Error(`best-temp-mail: 创建邮箱失败 HTTP ${res.status}`);
  }

  const json = await res.json() as {
    status?: string;
    data?: { address?: string; id?: string; update_tag?: string };
  };

  if (json.status !== 'success' || !json.data?.address) {
    throw new Error('best-temp-mail: 创建邮箱返回数据异常');
  }

  const { address, id, update_tag } = json.data;
  if (!id || !update_tag) {
    throw new Error('best-temp-mail: 返回数据缺少 id 或 update_tag');
  }

  /* 将 intToken、id、update_tag 序列化为 JSON 存入 token */
  const tokenData: BestTempMailToken = { intToken, id, update_tag };
  const token = JSON.stringify(tokenData);

  return { channel: CHANNEL, email: address, token };
}

/**
 * 获取 BestTempMail 邮箱的邮件列表
 *
 * 流程:
 * 1. 从 token 中反序列化出 intToken、id、update_tag
 * 2. POST /getEmailList 获取邮件列表
 * 3. 标准化邮件数据
 */
export async function getEmails(email: string, token: string): Promise<Email[]> {
  if (!email?.trim()) {
    throw new Error('best-temp-mail: 邮箱地址为空');
  }
  if (!token?.trim()) {
    throw new Error('best-temp-mail: token 为空');
  }

  /* 解析 token */
  let tokenData: BestTempMailToken;
  try {
    tokenData = JSON.parse(token) as BestTempMailToken;
  } catch {
    throw new Error('best-temp-mail: 无效的 token 格式');
  }

  if (!tokenData.intToken || !tokenData.id || !tokenData.update_tag) {
    throw new Error('best-temp-mail: token 数据不完整');
  }

  const res = await fetchWithTimeout(`${BASE}/getEmailList`, {
    method: 'POST',
    headers: HEADERS,
    body: JSON.stringify({
      address: email.trim(),
      id: tokenData.id,
      intToken: tokenData.intToken,
      update_tag: tokenData.update_tag,
    }),
  });
  if (!res.ok) {
    throw new Error(`best-temp-mail: 获取邮件失败 HTTP ${res.status}`);
  }

  const json = await res.json() as {
    status?: string;
    data?: { hasNewEmail?: boolean; emails?: any[] };
  };

  if (json.status !== 'success') {
    throw new Error('best-temp-mail: 获取邮件返回数据异常');
  }

  /* 无新邮件时返回空数组 */
  if (!json.data?.hasNewEmail || !Array.isArray(json.data.emails)) {
    return [];
  }

  return json.data.emails.map((raw: unknown) => normalizeEmail(raw, email));
}
