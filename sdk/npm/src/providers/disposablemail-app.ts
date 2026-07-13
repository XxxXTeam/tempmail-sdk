/**
 * DisposableMail.app 临时邮箱渠道
 * 网站: disposablemail.app
 * 域名: @disposablemail.dev, @mailmehere.cc
 * 纯 REST JSON API，无需认证
 */
import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "disposablemail-app";
const BASE = "https://disposablemail.app/api";

/** 请求头 */
const HEADERS: Record<string, string> = {
  "Content-Type": "application/json",
  Accept: "application/json",
  "User-Agent":
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
  Referer: "https://disposablemail.app/",
  Origin: "https://disposablemail.app",
};

/**
 * 创建 DisposableMail.app 临时邮箱
 *
 * 流程:
 * 1. POST /inbox 创建收件箱
 * 2. 从响应中提取 address 和 token
 * 3. token 直接存储 API 返回的字符串
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const res = await fetchWithTimeout(`${BASE}/inbox`, {
    method: "POST",
    headers: HEADERS,
    body: JSON.stringify({}),
  });
  if (!res.ok) {
    throw new Error(`disposablemail-app: 创建邮箱失败 HTTP ${res.status}`);
  }

  const json = (await res.json()) as {
    address?: string;
    token?: string;
    expiresAt?: string;
    createdAt?: string;
  };

  if (!json.address) {
    throw new Error("disposablemail-app: 创建邮箱返回数据缺少 address");
  }
  if (!json.token) {
    throw new Error("disposablemail-app: 创建邮箱返回数据缺少 token");
  }

  return {
    channel: CHANNEL,
    email: json.address,
    token: json.token,
    expiresAt: json.expiresAt,
    createdAt: json.createdAt,
  };
}

/**
 * 获取 DisposableMail.app 邮箱的邮件列表
 *
 * 流程:
 * 1. GET /inbox/emails?token={token} 获取邮件列表
 * 2. 标准化邮件数据
 */
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  if (!email?.trim()) {
    throw new Error("disposablemail-app: 邮箱地址为空");
  }
  if (!token?.trim()) {
    throw new Error("disposablemail-app: token 为空");
  }

  const res = await fetchWithTimeout(
    `${BASE}/inbox/emails?token=${encodeURIComponent(token.trim())}`,
    {
      method: "GET",
      headers: HEADERS,
    },
  );
  if (!res.ok) {
    throw new Error(`disposablemail-app: 获取邮件失败 HTTP ${res.status}`);
  }

  const json = (await res.json()) as {
    emails?: any[];
    total?: number;
  };

  if (!Array.isArray(json.emails) || json.emails.length === 0) {
    return [];
  }

  return json.emails.map((raw: unknown) => normalizeEmail(raw, email));
}
