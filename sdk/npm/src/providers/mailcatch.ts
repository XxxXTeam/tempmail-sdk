/**
 * MailCatch 临时邮箱渠道
 * 网站: mailcatch.com
 * 公开收件箱，无需认证/Session
 * API 返回 HTML 片段，使用正则提取数据
 */
import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";
import { randomInt } from "crypto";

const CHANNEL: Channel = "mailcatch";
const BASE_URL = "https://mailcatch.com";
const DOMAIN = "mailcatch.com";

/** 公共请求头 */
const COMMON_HEADERS: Record<string, string> = {
  Accept: "text/html, */*",
  "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
  "User-Agent":
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
  Referer: `${BASE_URL}/`,
};

/** 邮件项正则: 提取 data-email-id, data-subject, data-timestamp, data-sender */
const EMAIL_ITEM_RE =
  /<div\s+class="email-item"\s+data-email-id="([^"]*)"\s+data-subject="([^"]*)"\s+data-timestamp="([^"]*)"\s+data-sender="([^"]*)"/g;

/**
 * 生成随机用户名
 * 前缀 sdk + 16 位随机字母数字
 */
function randomUsername(): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  let out = "sdk";
  for (let i = 0; i < 16; i++) {
    out += chars[randomInt(chars.length)];
  }
  return out;
}

/**
 * 创建 MailCatch 临时邮箱
 *
 * 无需 API 调用，直接生成随机用户名 + @mailcatch.com
 * Token 存储用户名（@前部分），用于后续获取邮件
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const username = randomUsername();
  return {
    channel: CHANNEL,
    email: `${username}@${DOMAIN}`,
    token: username,
  };
}

/**
 * 获取 MailCatch 邮箱的邮件列表
 *
 * 流程:
 * 1. GET /api/list/{username} -> HTML 片段，正则提取邮件元数据
 * 2. 对每封邮件 GET /api/data/{username}/{emailId} -> HTML 邮件正文
 *
 * @param token - 用户名（@前部分）
 * @param email - 完整邮箱地址
 */
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  if (!token?.trim()) {
    throw new Error("mailcatch: token 为空");
  }

  const username = token.trim();

  /* 步骤 1: 获取邮件列表 HTML */
  const listUrl = `${BASE_URL}/api/list/${encodeURIComponent(username)}`;
  const r = await fetchWithTimeout(listUrl, {
    headers: COMMON_HEADERS,
  });
  if (!r.ok) {
    throw new Error(`mailcatch: 获取邮件列表失败 HTTP ${r.status}`);
  }

  const html = await r.text();

  /* 正则提取所有邮件项 */
  const items: Array<{
    id: string;
    subject: string;
    timestamp: string;
    sender: string;
  }> = [];

  let match: RegExpExecArray | null;
  /* 重置正则状态 */
  EMAIL_ITEM_RE.lastIndex = 0;
  while ((match = EMAIL_ITEM_RE.exec(html)) !== null) {
    items.push({
      id: match[1],
      subject: match[2],
      timestamp: match[3],
      sender: match[4],
    });
  }

  if (items.length === 0) {
    return [];
  }

  /* 步骤 2: 获取每封邮件的详细内容 */
  const emails: Email[] = [];
  for (const item of items) {
    if (!item.id) continue;

    const detailUrl = `${BASE_URL}/api/data/${encodeURIComponent(username)}/${encodeURIComponent(item.id)}`;
    let htmlBody = "";
    try {
      const rd = await fetchWithTimeout(detailUrl, {
        headers: COMMON_HEADERS,
      });
      if (rd.ok) {
        htmlBody = await rd.text();
      }
    } catch {
      /* 获取邮件详情失败，跳过正文 */
    }

    const mailData: Record<string, unknown> = {
      id: item.id,
      from: item.sender,
      to: email,
      subject: item.subject,
      html: htmlBody || "",
      date: item.timestamp,
    };

    emails.push(normalizeEmail(mailData, email));
  }

  return emails;
}
