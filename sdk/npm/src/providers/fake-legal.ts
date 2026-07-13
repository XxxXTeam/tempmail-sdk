import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "fake-legal";
const BASE = "https://imgui.de";

const DEFAULT_HEADERS: Record<string, string> = {
  Accept: "application/json, text/plain, */*",
  "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
  "Cache-Control": "no-cache",
  DNT: "1",
  Pragma: "no-cache",
  Referer: "https://imgui.de/",
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

interface DomainsResponse {
  domains?: string[];
}

interface NewInboxResponse {
  success?: boolean;
  address?: string;
  expiresIn?: string;
}

interface InboxResponse {
  success?: boolean;
  emails?: unknown[];
}

async function fetchDomains(): Promise<string[]> {
  const response = await fetchWithTimeout(`${BASE}/api/domains`, {
    method: "GET",
    headers: DEFAULT_HEADERS,
  });
  if (!response.ok) {
    throw new Error(`fake-legal: 获取域名列表失败 HTTP ${response.status}`);
  }
  const data = (await response.json()) as DomainsResponse;
  const list =
    data.domains?.filter((d) => typeof d === "string" && d.trim()) ?? [];
  if (list.length === 0) {
    throw new Error("fake-legal: 无可用域名");
  }
  return list;
}

function pickDomain(domains: string[], preferred?: string | null): string {
  if (preferred && typeof preferred === "string") {
    const p = preferred.trim().toLowerCase();
    const hit = domains.find((d) => d.toLowerCase() === p);
    if (hit) return hit;
    throw new Error(`fake-legal: domain not available: ${p}`);
  }
  return domains[Math.floor(Math.random() * domains.length)];
}

function generateRandomUsername(length: number): string {
  const chars =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  let result = "";
  for (let i = 0; i < length; i++) {
    result += chars.charAt(Math.floor(Math.random() * chars.length));
  }
  return result;
}

/**
 * 创建收件箱：fake-legal 用 GET /api/inbox/new，imgui-de/pulsewebmenu-de 用 POST /api/inbox/custom
 */
export async function generateEmail(
  domain?: string | null,
  channel: Channel = CHANNEL,
): Promise<InternalEmailInfo> {
  const domains = await fetchDomains();
  const d = pickDomain(domains, domain ?? undefined);

  let url: string;
  let method: string;
  let body: string | undefined;
  const headers = { ...DEFAULT_HEADERS };

  if (d === "imgui.de" || d === "pulsewebmenu.de") {
    url = `${BASE}/api/inbox/custom`;
    method = "POST";
    const username = generateRandomUsername(12);
    body = JSON.stringify({ username, domain: d });
    headers["Content-Type"] = "application/json";
  } else {
    url = `${BASE}/api/inbox/new?domain=${encodeURIComponent(d)}`;
    method = "GET";
  }

  const response = await fetchWithTimeout(url, {
    method,
    headers,
    body,
  });
  if (!response.ok) {
    throw new Error(`fake-legal: 创建收件箱失败 HTTP ${response.status}`);
  }
  const data = (await response.json()) as NewInboxResponse;
  if (!data.success || !data.address?.trim()) {
    throw new Error("fake-legal: 创建收件箱返回无效");
  }
  return {
    channel,
    email: data.address.trim(),
  };
}

/**
 * GET /api/inbox/{encodeURIComponent(email)}
 */
export async function getEmails(email: string): Promise<Email[]> {
  if (!email?.trim()) {
    throw new Error("fake-legal: 邮箱地址为空");
  }
  const path = encodeURIComponent(email.trim());
  const response = await fetchWithTimeout(`${BASE}/api/inbox/${path}`, {
    method: "GET",
    headers: DEFAULT_HEADERS,
  });
  if (!response.ok) {
    throw new Error(`fake-legal: 获取邮件失败 HTTP ${response.status}`);
  }
  const data = (await response.json()) as InboxResponse;
  if (!data.success || !Array.isArray(data.emails)) {
    return [];
  }
  return data.emails.map((raw) => normalizeEmail(raw, email.trim()));
}
