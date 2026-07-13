import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "tempmail365";
const BASE = "https://tempmail365.cn/tempemail.php";

/** 可用域名列表 */
const DOMAINS = ["fengyou.cc", "shop345.com", "nutemail.com", "qvrf.cn"];

const DEFAULT_HEADERS: Record<string, string> = {
  Accept: "application/json, text/plain, */*",
  "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
  "Cache-Control": "no-cache",
  DNT: "1",
  Pragma: "no-cache",
  Referer: "https://tempmail365.cn/",
  "sec-ch-ua":
    '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
  "sec-ch-ua-mobile": "?0",
  "sec-ch-ua-platform": '"Windows"',
  "sec-fetch-dest": "empty",
  "sec-fetch-mode": "cors",
  "sec-fetch-site": "same-origin",
  "User-Agent":
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
};

interface ConfigResponse {
  domains?: string[];
}

interface CreateEmailResponse {
  success?: boolean;
}

interface FetchMailResponse {
  content?: string;
}

/** 生成8位随机字母数字用户名 */
function randomUsername(len = 8): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  let result = "";
  for (let i = 0; i < len; i++) {
    result += chars[Math.floor(Math.random() * chars.length)];
  }
  return result;
}

/**
 * 从HTML邮件内容中提取发件人
 * 匹配 "发件人:" 或 "From:" 后面的文本
 */
function extractSender(html: string): string {
  const match = html.match(/(?:发件人|From)\s*[:：]\s*([^\n<]+)/i);
  return match ? match[1].trim() : "unknown";
}

/**
 * 从HTML邮件内容中提取主题
 * 匹配 "主题:" 或 "Subject:" 后面的文本
 */
function extractSubject(html: string): string {
  const match = html.match(/(?:主题|Subject)\s*[:：]\s*([^\n<]+)/i);
  return match ? match[1].trim() : "(无主题)";
}

/**
 * 获取可用域名列表
 */
async function fetchDomains(): Promise<string[]> {
  const response = await fetchWithTimeout(`${BASE}?action=get_config`, {
    method: "GET",
    headers: DEFAULT_HEADERS,
  });
  if (!response.ok) {
    throw new Error(`tempmail365: 获取域名列表失败 HTTP ${response.status}`);
  }
  const data = (await response.json()) as ConfigResponse;
  const list =
    data.domains?.filter((d) => typeof d === "string" && d.trim()) ?? [];
  if (list.length === 0) {
    // 接口返回为空时回退到硬编码域名
    return DOMAINS;
  }
  return list;
}

/**
 * 创建临时邮箱
 * 流程：获取域名列表 → 随机选域名 → 随机生成用户名 → 调用 create_email
 */
export async function generateEmail(
  domain?: string | null,
  channel: Channel = CHANNEL,
): Promise<InternalEmailInfo> {
  let domains: string[];
  try {
    domains = await fetchDomains();
  } catch {
    // 获取域名失败时回退到硬编码列表
    domains = DOMAINS;
  }

  const d =
    domain?.trim() || domains[Math.floor(Math.random() * domains.length)];
  const user = randomUsername();
  const email = `${user}@${d}`;

  const url = `${BASE}?action=create_email&email=${encodeURIComponent(email)}&domain=${encodeURIComponent(d)}`;
  const response = await fetchWithTimeout(url, {
    method: "GET",
    headers: DEFAULT_HEADERS,
  });
  if (!response.ok) {
    throw new Error(`tempmail365: 创建邮箱失败 HTTP ${response.status}`);
  }
  const data = (await response.json()) as CreateEmailResponse;
  if (!data.success) {
    throw new Error("tempmail365: 创建邮箱返回失败");
  }

  return {
    channel,
    email,
  };
}

/**
 * 获取邮件列表
 * 流程：fetch_mail → 判断是否有邮件 → 提取发件人/主题 → normalize
 */
export async function getEmails(email: string): Promise<Email[]> {
  if (!email?.trim()) {
    throw new Error("tempmail365: 邮箱地址为空");
  }

  const addr = email.trim();
  const url = `${BASE}?action=fetch_mail&email=${encodeURIComponent(addr)}`;
  const response = await fetchWithTimeout(url, {
    method: "GET",
    headers: DEFAULT_HEADERS,
  });
  if (!response.ok) {
    throw new Error(`tempmail365: 获取邮件失败 HTTP ${response.status}`);
  }

  const data = (await response.json()) as FetchMailResponse;
  const content = data.content ?? "";

  // "无邮件" 表示收件箱为空
  if (!content || content === "无邮件") {
    return [];
  }

  // 从HTML内容中提取发件人和主题
  const sender = extractSender(content);
  const subject = extractSubject(content);

  // 构造邮件对象并经过标准化处理
  const rawEmail = {
    from: sender,
    to: addr,
    subject,
    body: content,
    html: content,
    date: new Date().toISOString(),
  };

  return [normalizeEmail(rawEmail, addr)];
}
