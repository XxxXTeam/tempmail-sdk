/**
 * linshiyouxiang.net 临时邮箱渠道
 *
 * 流程：
 *   1. GET / 从 HTML 正则提取 tempMailGlobal（邮箱）和 mailCodeGlobal（校验 code）
 *   2. POST /get-messages  body: {"email":"...","code":"..."}
 * token 存储 mailCodeGlobal 的值（后续请求用于校验，请求体自带 email+code，不依赖 cookie）。
 */
import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'linshiyouxiang-net';
const BASE_URL = 'https://www.linshiyouxiang.net';

/** 提取首页 HTML 中的 tempMailGlobal（邮箱地址） */
const EMAIL_RE = /tempMailGlobal\s*=\s*'([^']+)'/;
/** 提取首页 HTML 中的 mailCodeGlobal（校验 code） */
const CODE_RE = /mailCodeGlobal\s*=\s*'([^']+)'/;

const USER_AGENT =
  'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0';

interface GetMessagesResponse {
  emails?: Record<string, unknown>[] | null;
  success?: boolean;
}

/**
 * 创建 linshiyouxiang.net 临时邮箱
 * GET / 获取首页 HTML，正则提取 tempMailGlobal（邮箱）与 mailCodeGlobal（校验 code）。
 * token 存储 mailCodeGlobal。
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const r = await fetchWithTimeout(`${BASE_URL}/`, {
    headers: {
      Accept: 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
      'Accept-Language': 'zh-CN,zh;q=0.9,en;q=0.8',
      'User-Agent': USER_AGENT,
    },
  });
  if (!r.ok) {
    throw new Error(`linshiyouxiang-net: 请求首页失败 HTTP ${r.status}`);
  }

  const html = await r.text();

  const emailMatch = EMAIL_RE.exec(html);
  if (!emailMatch?.[1]) {
    throw new Error('linshiyouxiang-net: 未能从首页提取邮箱地址');
  }
  const email = emailMatch[1].trim();
  if (!email) {
    throw new Error('linshiyouxiang-net: 提取的邮箱地址为空');
  }

  const codeMatch = CODE_RE.exec(html);
  const code = codeMatch?.[1] ? codeMatch[1].trim() : '';

  return { channel: CHANNEL, email, token: code };
}

/**
 * 获取 linshiyouxiang.net 邮件列表
 * POST /get-messages  body: {"email":"...","code":"<token>"}
 * 响应 {"emails":null|[...],"success":true}
 */
export async function getEmails(token: string, email: string): Promise<Email[]> {
  const addr = email.trim();
  if (!addr) {
    throw new Error('linshiyouxiang-net: 邮箱地址为空');
  }

  const r = await fetchWithTimeout(`${BASE_URL}/get-messages`, {
    method: 'POST',
    headers: {
      Accept: 'application/json, text/javascript, */*; q=0.01',
      'Accept-Language': 'zh-CN,zh;q=0.9,en;q=0.8',
      Referer: `${BASE_URL}/`,
      Origin: BASE_URL,
      'User-Agent': USER_AGENT,
      'Content-Type': 'application/json',
      'X-Requested-With': 'XMLHttpRequest',
    },
    body: JSON.stringify({ email: addr, code: token ?? '' }),
  });
  if (!r.ok) {
    throw new Error(`linshiyouxiang-net: 请求获取邮件失败 HTTP ${r.status}`);
  }

  let result: GetMessagesResponse;
  try {
    result = (await r.json()) as GetMessagesResponse;
  } catch {
    throw new Error('linshiyouxiang-net: 解析邮件列表失败');
  }

  const list = result.emails ?? [];
  if (!Array.isArray(list) || list.length === 0) {
    return [];
  }

  return list.map((raw) => normalizeEmail(raw, addr));
}
