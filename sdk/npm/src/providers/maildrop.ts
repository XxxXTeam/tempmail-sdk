import { InternalEmailInfo, Email, Channel } from "../types";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "maildrop";
const BASE = "https://maildrop.cx";
const EXCLUDED_SUFFIX = "transformer.edu.kg";
const DEFAULT_HEADERS: Record<string, string> = {
  Accept: "application/json, text/plain, */*",
  "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
  "Cache-Control": "no-cache",
  DNT: "1",
  Pragma: "no-cache",
  Referer: "https://maildrop.cx/zh-cn/app",
  "sec-ch-ua":
    '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
  "sec-ch-ua-mobile": "?0",
  "sec-ch-ua-platform": '"Windows"',
  "sec-fetch-dest": "empty",
  "sec-fetch-mode": "cors",
  "sec-fetch-site": "same-origin",
  "User-Agent":
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
  "x-requested-with": "XMLHttpRequest",
};

function randomLocalPart(length = 10): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  let s = "";
  for (let i = 0; i < length; i++) {
    s += chars.charAt(Math.floor(Math.random() * chars.length));
  }
  return s;
}

async function fetchSuffixes(): Promise<string[]> {
  const response = await fetchWithTimeout(`${BASE}/api/suffixes.php`, {
    method: "GET",
    headers: DEFAULT_HEADERS,
  });
  if (!response.ok) {
    throw new Error(`maildrop: 获取后缀列表失败 HTTP ${response.status}`);
  }
  const data = (await response.json()) as unknown;
  if (!Array.isArray(data)) {
    throw new Error("maildrop: 后缀列表格式无效");
  }
  const ex = EXCLUDED_SUFFIX.toLowerCase();
  const list = data
    .filter((x): x is string => typeof x === "string" && x.trim().length > 0)
    .map((d) => d.trim())
    .filter((d) => d.toLowerCase() !== ex);
  if (list.length === 0) {
    throw new Error("maildrop: 无可用域名");
  }
  return list;
}

function pickSuffix(suffixes: string[], preferred?: string | null): string {
  if (preferred && typeof preferred === "string") {
    const p = preferred.trim().toLowerCase();
    const hit = suffixes.find((d) => d.toLowerCase() === p);
    if (hit) return hit;
    throw new Error(`maildrop: domain not available: ${p}`);
  }
  return suffixes[Math.floor(Math.random() * suffixes.length)];
}

function cxDateToIso(dateStr: string): string {
  const s = dateStr?.trim() ?? "";
  if (/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/.test(s)) {
    return s.replace(" ", "T") + "Z";
  }
  return s;
}

interface EmailsApiRow {
  id?: number | string;
  from_addr?: string;
  subject?: string;
  date?: string;
  isRead?: number | boolean;
  description?: string;
}

interface EmailsApiResponse {
  emails?: EmailsApiRow[];
}

export async function generateEmail(
  domain?: string | null,
): Promise<InternalEmailInfo> {
  const suffixes = await fetchSuffixes();
  const dom = pickSuffix(suffixes, domain ?? undefined);
  const local = randomLocalPart();
  const email = `${local}@${dom}`;
  return {
    channel: CHANNEL,
    email,
    token: email,
  };
}

/**
 * 详情接口响应结构（从前端代码确认）：
 * - content: 完整 HTML 正文
 * - subject / from_addr / date: 邮件元数据
 * - attachment: JSON 字符串数组 [{filename, path, size}]（可能为空）
 */
interface EmailContentResponse {
  content?: string;
  subject?: string;
  from_addr?: string;
  date?: string;
  attachment?: string;
}

/**
 * 附件条目结构（详情接口 attachment 字段解析后）
 */
interface AttachmentItem {
  filename?: string;
  path?: string;
  size?: number;
}

/**
 * 通过详情接口获取单封邮件完整内容
 * GET /api/email_content.php?id={id}
 * 失败时返回 null，调用方回退到列表 description
 */
async function fetchEmailDetail(
  id: string,
): Promise<EmailContentResponse | null> {
  const trimmedId = String(id || "").trim();
  if (!trimmedId) return null;
  const qs = new URLSearchParams({ id: trimmedId });
  try {
    const res = await fetchWithTimeout(
      `${BASE}/api/email_content.php?${qs}`,
      {
        method: "GET",
        headers: DEFAULT_HEADERS,
      },
    );
    if (!res.ok) return null;
    return (await res.json()) as EmailContentResponse;
  } catch {
    return null;
  }
}

/**
 * 解析详情接口的 attachment 字段（JSON 字符串）
 * 返回归一化附件列表
 */
function parseAttachments(
  raw: string | undefined,
): Array<{ filename: string; size?: number }> {
  if (!raw || typeof raw !== "string" || !raw.trim()) return [];
  try {
    const items = JSON.parse(raw) as AttachmentItem[];
    if (!Array.isArray(items)) return [];
    return items
      .filter((it) => it && typeof it.filename === "string" && it.filename.trim())
      .map((it) => ({
        filename: String(it.filename).trim(),
        size: typeof it.size === "number" ? it.size : undefined,
      }));
  } catch {
    return [];
  }
}

/**
 * 获取邮件列表并补拉详情
 * 流程:
 *   1. GET /api/emails.php?addr={email} 拉取列表（仅含 description 摘要）
 *   2. 对每封邮件 GET /api/email_content.php?id={id} 拉取详情（含 content 完整 HTML）
 *   3. 详情失败时保留列表 description 作为回退
 */
export async function getEmails(
  _token: string,
  email: string,
): Promise<Email[]> {
  const addr = email?.trim();
  if (!addr) {
    throw new Error("maildrop: 邮箱地址为空");
  }
  const qs = new URLSearchParams({ addr, page: "1", limit: "20" });
  const response = await fetchWithTimeout(`${BASE}/api/emails.php?${qs}`, {
    method: "GET",
    headers: DEFAULT_HEADERS,
  });
  if (!response.ok) {
    throw new Error(`maildrop: 获取邮件失败 HTTP ${response.status}`);
  }
  const data = (await response.json()) as EmailsApiResponse;
  const rows = Array.isArray(data.emails) ? data.emails : [];

  const out: Email[] = [];
  for (const item of rows) {
    const id = String(item.id ?? "");
    const desc = item.description?.trim() ?? "";
    const isRead = item.isRead === true || item.isRead === 1;

    /* 默认使用列表字段构造 */
    let from = item.from_addr?.trim() ?? "";
    let subject = item.subject?.trim() ?? "";
    let date = cxDateToIso(item.date ?? "");
    let html = "";
    let attachments: Array<{ filename: string; size?: number }> = [];

    /* 拉取详情覆盖 html/attachments/元数据 */
    if (id) {
      const detail = await fetchEmailDetail(id);
      if (detail) {
        if (typeof detail.content === "string" && detail.content.trim()) {
          html = detail.content;
        }
        if (typeof detail.from_addr === "string" && detail.from_addr.trim()) {
          from = detail.from_addr.trim();
        }
        if (typeof detail.subject === "string" && detail.subject.trim()) {
          subject = detail.subject.trim();
        }
        if (typeof detail.date === "string" && detail.date.trim()) {
          date = cxDateToIso(detail.date);
        }
        attachments = parseAttachments(detail.attachment);
      }
    }

    out.push({
      id,
      from,
      to: addr,
      subject,
      text: desc,
      html,
      date,
      isRead,
      attachments,
    });
  }
  return out;
}
