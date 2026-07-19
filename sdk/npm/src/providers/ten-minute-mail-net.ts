import { InternalEmailInfo, Email } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const BASE_URL = "https://10minutemail.net";

interface AddressResponse {
  mail_get_mail: string;
  mail_list: Array<{ mail_id: string; from: string; subject: string; datetime: string }>;
}

interface DetailBody {
  content: string;
  body: string;
}

interface DetailResponse {
  from: string;
  to: string;
  subject: string;
  datetime: string;
  body: DetailBody[];
  plain: string[];
}

/**
 * 创建 10minutemail.net 临时邮箱
 * 通过 PHPSESSID cookie 获取分配的邮箱
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const response = await fetchWithTimeout(`${BASE_URL}/address.api.php`, {
    method: "GET",
    headers: {
      Accept: "application/json, text/plain, */*",
    },
  });
  if (!response.ok) {
    throw new Error(`ten-minute-mail-net: 创建邮箱失败 ${response.status}`);
  }

  const setCookie = response.headers.get("set-cookie") || "";
  const match = setCookie.match(/PHPSESSID=([^;]+)/);
  if (!match) {
    throw new Error("ten-minute-mail-net: 响应中未找到 PHPSESSID cookie");
  }
  const cookie = `PHPSESSID=${match[1]}`;

  const json = (await response.json()) as AddressResponse;
  if (!json.mail_get_mail) {
    throw new Error("ten-minute-mail-net: 响应缺少 mail_get_mail 字段");
  }

  return {
    channel: "ten-minute-mail-net",
    email: json.mail_get_mail,
    token: JSON.stringify({ cookie }),
  };
}

/**
 * 获取 10minutemail.net 邮件列表
 * @param token - JSON 字符串包含 cookie
 * @param email - 邮箱地址
 */
export async function getEmails(token: string, email: string): Promise<Email[]> {
  if (!token) {
    throw new Error("ten-minute-mail-net: token 不能为空");
  }

  let cookie = "";
  try {
    const parsed = JSON.parse(token) as { cookie?: string };
    cookie = String(parsed.cookie || "").trim();
  } catch {
    throw new Error("ten-minute-mail-net: token 格式无效");
  }
  if (!cookie) {
    throw new Error("ten-minute-mail-net: token 缺少 cookie 字段");
  }

  const listResp = await fetchWithTimeout(`${BASE_URL}/address.api.php`, {
    method: "GET",
    headers: {
      Accept: "application/json, text/plain, */*",
      Cookie: cookie,
    },
  });
  if (!listResp.ok) {
    throw new Error(`ten-minute-mail-net: 获取列表失败 ${listResp.status}`);
  }

  const listJson = (await listResp.json()) as AddressResponse;
  if (!listJson.mail_list || listJson.mail_list.length === 0) {
    return [];
  }

  const emails: Email[] = [];
  for (const item of listJson.mail_list) {
    try {
      const detailResp = await fetchWithTimeout(
        `${BASE_URL}/mail.api.php?mailid=${encodeURIComponent(item.mail_id)}`,
        {
          method: "GET",
          headers: {
            Accept: "application/json, text/plain, */*",
            Cookie: cookie,
          },
        },
      );
      if (!detailResp.ok) continue;

      const detail = (await detailResp.json()) as DetailResponse;
      let text = "";
      let html = "";
      for (const b of detail.body || []) {
        if (b.content === "text/plain") text = b.body;
        if (b.content === "text/html") html = b.body;
      }
      if (!text && detail.plain?.length) {
        text = detail.plain[0];
      }

      emails.push(
        normalizeEmail(
          {
            id: item.mail_id,
            from: detail.from || item.from,
            to: email,
            subject: detail.subject || item.subject,
            text,
            html,
            date: detail.datetime || item.datetime,
          },
          email,
        ),
      );
    } catch {
      continue;
    }
  }

  return emails;
}
