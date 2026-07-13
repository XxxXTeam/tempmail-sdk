import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";
import { getConfig } from "../config";

const CHANNEL: Channel = "apihz";
const BASE_URL = "https://cn.apihz.cn";
const TOK_PREFIX = "apihz1:";

/** apihz 官方公共账号（共享频次），未配置自有 id/key 时使用 */
const PUBLIC_ID = "88888888";
const PUBLIC_KEY = "88888888";

const UA =
  "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36";

/** 可用收信域名（apihz 自研，MX 指向 mail.apimail.* 自建服务器） */
const DOMAINS = ["apimail.email", "apimail.vip"];

/** 读取 apihz 调用凭据：优先配置/环境变量，回退官方公共账号 */
function getCredentials(): { id: string; key: string } {
  const cfg = getConfig();
  return {
    id: cfg.apihzId || PUBLIC_ID,
    key: cfg.apihzKey || PUBLIC_KEY,
  };
}

function randomLocal(n: number): string {
  const chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  let r = "";
  for (let i = 0; i < n; i++) {
    r += chars[Math.floor(Math.random() * chars.length)];
  }
  return r;
}

/** 生成 8-20 位随机密码（读信必须携带创建时的密码） */
function randomPassword(): string {
  const chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  let r = "";
  for (let i = 0; i < 12; i++) {
    r += chars[Math.floor(Math.random() * chars.length)];
  }
  return r;
}

/** token 编码：前缀 + base64(JSON{mail,pwd})，读信时解出 pwd */
function encToken(mail: string, pwd: string): string {
  const raw = JSON.stringify({ mail, pwd });
  return TOK_PREFIX + Buffer.from(raw, "utf8").toString("base64");
}

function decToken(tok: string): { mail: string; pwd: string } {
  if (!tok.startsWith(TOK_PREFIX)) {
    throw new Error("apihz: 无效 token");
  }
  let raw: string;
  try {
    raw = Buffer.from(tok.slice(TOK_PREFIX.length), "base64").toString("utf8");
  } catch {
    throw new Error("apihz: 无效 token");
  }
  const o = JSON.parse(raw) as { mail?: string; pwd?: string };
  const mail = (o.mail ?? "").trim();
  const pwd = (o.pwd ?? "").trim();
  if (!mail || !pwd) throw new Error("apihz: 无效 token");
  return { mail, pwd };
}

/**
 * 创建 apihz（接口盒子）临时邮箱
 * GET /api/mail/mailcache.php?id=&key=&domain=&name=&pwd=&buytype=0
 * 有效期 10 分钟，读信必须携带创建时的 pwd
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const { id, key } = getCredentials();
  const domain = DOMAINS[Math.floor(Math.random() * DOMAINS.length)];
  const name = randomLocal(10);
  const pwd = randomPassword();

  const url =
    `${BASE_URL}/api/mail/mailcache.php?id=${encodeURIComponent(id)}&key=${encodeURIComponent(key)}` +
    `&domain=${encodeURIComponent(domain)}&name=${encodeURIComponent(name)}&pwd=${encodeURIComponent(pwd)}&buytype=0`;

  const resp = await fetchWithTimeout(url, {
    method: "GET",
    headers: { "User-Agent": UA, Accept: "application/json" },
  });

  if (!resp.ok) {
    throw new Error(`apihz: 创建邮箱失败 ${resp.status}`);
  }

  const data = (await resp.json()) as ApihzCreateResponse;
  if (data.code !== 200 || !data.mail) {
    throw new Error(`apihz: 创建邮箱失败 ${data.msg || JSON.stringify(data)}`);
  }

  /* 优先使用响应回传的 pwd（与请求一致），确保读信密码正确 */
  const finalPwd = data.pwd || pwd;

  return {
    channel: CHANNEL,
    email: data.mail,
    token: encToken(data.mail, finalPwd),
  };
}

/**
 * 获取 apihz 邮件列表
 * GET /api/mail/mailgetlist.php?id=&key=&mail=&pwd=&page=1
 */
export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  if (!token) {
    throw new Error("apihz: 缺少 token");
  }
  const { mail, pwd } = decToken(token);
  const { id, key } = getCredentials();
  const addr = mail || email;

  const url =
    `${BASE_URL}/api/mail/mailgetlist.php?id=${encodeURIComponent(id)}&key=${encodeURIComponent(key)}` +
    `&mail=${encodeURIComponent(addr)}&pwd=${encodeURIComponent(pwd)}&page=1`;

  const resp = await fetchWithTimeout(url, {
    method: "GET",
    headers: { "User-Agent": UA, Accept: "application/json" },
  });

  if (!resp.ok) {
    throw new Error(`apihz: 获取邮件失败 ${resp.status}`);
  }

  const data = (await resp.json()) as ApihzListResponse;
  if (data.code !== 200 || !Array.isArray(data.data)) {
    return [];
  }

  return data.data.map((msg, idx) =>
    normalizeEmail(
      {
        id: String(msg.time1 ?? idx),
        from: msg.frommail || msg.fromname || "",
        to: addr,
        subject: msg.subject || "",
        text: msg.content || "",
        html: msg.content || "",
        date: msg.time2 || (msg.time1 ? String(msg.time1) : ""),
        isRead: false,
        attachments: [],
      },
      addr,
    ),
  );
}

type ApihzCreateResponse = {
  code?: number;
  msg?: string;
  mail?: string;
  pwd?: string;
  endtime?: string;
};

type ApihzMessage = {
  frommail?: string;
  fromname?: string;
  subject?: string;
  time1?: number;
  time2?: string;
  content?: string;
};

type ApihzListResponse = {
  code?: number;
  msg?: string;
  num?: string | number;
  data?: ApihzMessage[];
};
