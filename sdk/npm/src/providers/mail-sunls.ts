/**
 * mail-sunls 渠道实现
 * 基于 mail.sunls.de 的临时邮箱服务
 * 无需 token / session，直接通过 API 获取域名并创建邮箱
 */
import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "mail-sunls";
const BASE_URL = "https://mail.sunls.de";

const HEADERS: Record<string, string> = {
  Accept: "application/json, text/plain, */*",
  "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
  "User-Agent":
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
  "sec-ch-ua":
    '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
  "sec-ch-ua-mobile": "?0",
  "sec-ch-ua-platform": '"Windows"',
  "sec-fetch-dest": "empty",
  "sec-fetch-mode": "cors",
  "sec-fetch-site": "same-origin",
  Referer: `${BASE_URL}/`,
};

/**
 * 生成指定长度的随机本地部分（小写字母 + 数字）
 */
function randomLocal(len: number): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  let s = "";
  for (let i = 0; i < len; i++)
    s += chars[Math.floor(Math.random() * chars.length)];
  return s;
}

/**
 * 获取可用域名列表
 * GET /api/domain → string[]
 */
async function fetchDomains(): Promise<string[]> {
  const res = await fetchWithTimeout(`${BASE_URL}/api/domain`, {
    method: "GET",
    headers: HEADERS,
  });
  if (!res.ok) {
    throw new Error(`mail-sunls: 获取域名列表失败 HTTP ${res.status}`);
  }
  const data = (await res.json()) as unknown;
  if (!Array.isArray(data)) {
    throw new Error("mail-sunls: 域名列表格式无效");
  }
  const list = data
    .filter((x): x is string => typeof x === "string" && x.trim().length > 0)
    .map((d) => d.trim());
  if (list.length === 0) {
    throw new Error("mail-sunls: 无可用域名");
  }
  return list;
}

/**
 * 创建临时邮箱
 * 无需 API 调用，直接用随机本地部分 + 随机域名拼接
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const domains = await fetchDomains();
  const domain = domains[Math.floor(Math.random() * domains.length)];
  const local = randomLocal(10);
  const email = `${local}@${domain}`;
  return { channel: CHANNEL, email };
}

/**
 * 获取邮件列表
 * GET /api/fetch?to={email} → 邮件数组
 * 对于列表中正文为空的邮件，尝试通过 GET /api/fetch/{id} 获取完整内容
 */
export async function getEmails(email: string): Promise<Email[]> {
  const addr = String(email || "").trim();
  if (!addr) {
    throw new Error("mail-sunls: 邮箱地址为空");
  }
  const qs = new URLSearchParams({ to: addr });
  const res = await fetchWithTimeout(`${BASE_URL}/api/fetch?${qs}`, {
    method: "GET",
    headers: HEADERS,
  });
  if (!res.ok) {
    throw new Error(`mail-sunls: 获取邮件失败 HTTP ${res.status}`);
  }
  const data = (await res.json()) as unknown;
  if (!Array.isArray(data)) {
    return [];
  }

  const out: Email[] = [];
  for (const item of data) {
    if (!item || typeof item !== "object") continue;
    const row = item as Record<string, any>;

    let text = row.text || row.body || "";
    let html = row.html || row.body_html || "";

    /* 如果列表项没有正文，尝试获取单封邮件详情 */
    const mailId = row.id || row._id || "";
    if (mailId && !text && !html) {
      try {
        const detailRes = await fetchWithTimeout(
          `${BASE_URL}/api/fetch/${encodeURIComponent(String(mailId))}`,
          {
            method: "GET",
            headers: HEADERS,
          },
        );
        if (detailRes.ok) {
          const detail = (await detailRes.json()) as Record<string, any>;
          text = detail.text || detail.body || text;
          html = detail.html || detail.body_html || html;
        }
      } catch {
        /* 详情获取失败不影响列表返回 */
      }
    }

    out.push(
      normalizeEmail(
        {
          id: String(mailId),
          from: row.from || row.from_address || row.sender || "",
          to: addr,
          subject: row.subject || "",
          text,
          html,
          date: row.date || row.received_at || row.created_at || "",
          isRead: false,
          attachments: row.attachments || [],
        },
        addr,
      ),
    );
  }
  return out;
}
