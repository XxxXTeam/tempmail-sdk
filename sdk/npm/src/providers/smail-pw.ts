import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'smail-pw';
const ROOT_DATA_URL = 'https://smail.pw/_root.data';

const DEFAULT_HEADERS: Record<string, string> = {
  Accept: '*/*',
  'accept-language': 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
  'cache-control': 'no-cache',
  dnt: '1',
  origin: 'https://smail.pw',
  pragma: 'no-cache',
  referer: 'https://smail.pw/',
  'sec-ch-ua':
    '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
  'sec-ch-ua-mobile': '?0',
  'sec-ch-ua-platform': '"Windows"',
  'sec-fetch-dest': 'empty',
  'sec-fetch-mode': 'cors',
  'sec-fetch-site': 'same-origin',
  'User-Agent':
    'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
};

/** Flight/RSC 根为数组时，按出现顺序展开嵌套数组，保留标量与对象（不展开对象键），用于解析交错键值 */
function flightLinearLeaves(node: unknown): unknown[] {
  if (!Array.isArray(node)) {
    return [];
  }
  const out: unknown[] = [];
  for (const el of node) {
    if (Array.isArray(el)) {
      out.push(...flightLinearLeaves(el));
    } else {
      out.push(el);
    }
  }
  return out;
}

const MAIL_KV_KEYS = new Set(['id', 'to_address', 'from_name', 'from_address']);

/**
 * 在线性序列中查找 ... "subject", <str>, "time", <num>，再向前成对读取已知字段。
 * 比纯正则更耐受 emails 与 id 之间的 {"_23":4,...} 引用块。
 */
function parseSmailEmailsFromLinear(
  leaves: unknown[],
  recipientEmail: string,
): any[] {
  const mails: any[] = [];
  for (let i = 0; i + 3 < leaves.length; i++) {
    if (leaves[i] !== 'subject') {
      continue;
    }
    const subj = leaves[i + 1];
    if (typeof subj !== 'string') {
      continue;
    }
    if (leaves[i + 2] !== 'time') {
      continue;
    }
    const timeRaw = leaves[i + 3];
    const timeMs =
      typeof timeRaw === 'number'
        ? timeRaw
        : typeof timeRaw === 'string' && /^\d+$/.test(timeRaw)
          ? parseInt(timeRaw, 10)
          : NaN;
    if (!Number.isFinite(timeMs)) {
      continue;
    }

    const fields: Record<string, string> = {};
    for (let k = i - 2; k >= 1; k -= 2) {
      const key = leaves[k];
      const val = leaves[k + 1];
      if (typeof key !== 'string' || typeof val !== 'string') {
        break;
      }
      if (MAIL_KV_KEYS.has(key)) {
        fields[key] = val;
      }
      /* 未知键值对跳过（RSC 可能在 subject 前插入其它字符串字段） */
    }

    mails.push({
      id: fields.id || '',
      to_address: fields.to_address || recipientEmail,
      from_name: fields.from_name || '',
      from_address: fields.from_address || '',
      subject: subj,
      date: timeMs,
      text: '',
      html: '',
      attachments: [],
    });
  }
  return mails;
}

function extractSessionCookie(response: Response): string {
  const h = response.headers as Headers & { getSetCookie?: () => string[] };
  if (typeof h.getSetCookie === 'function') {
    for (const line of h.getSetCookie()) {
      const m = line.match(/^__session=([^;]+)/);
      if (m) return `__session=${m[1]}`;
    }
  }
  const single = response.headers.get('set-cookie');
  if (single) {
    const m = single.match(/__session=([^;]+)/);
    if (m) return `__session=${m[1]}`;
  }
  return '';
}

/** RSC/Flight 风格 JSON 文本中提取 @smail.pw 收件地址 */
function extractInboxEmail(text: string): string | null {
  const quoted = text.match(/"([a-z0-9][a-z0-9.-]*@smail\.pw)"/i);
  if (quoted) return quoted[1];
  const plain = text.match(/\b([a-z0-9][a-z0-9.-]*@smail\.pw)\b/i);
  return plain ? plain[1] : null;
}

/** 从响应体解析邮件条目（正则路径，与早期 curl 样本一致） */
function parseSmailEmailsRegex(text: string, recipientEmail: string): any[] {
  const out: any[] = [];
  const re =
    /"id","([^"]+)","to_address","([^"]*)","from_name","([^"]*)","from_address","([^"]*)","subject","([^"]*)","time",(\d+)/g;
  let m: RegExpExecArray | null;
  while ((m = re.exec(text)) !== null) {
    out.push({
      id: m[1],
      to_address: m[2] || recipientEmail,
      from_name: m[3],
      from_address: m[4],
      subject: m[5],
      date: parseInt(m[6], 10),
      text: '',
      html: '',
      attachments: [],
    });
  }
  if (out.length > 0) {
    return out;
  }

  const re2 =
    /"id","([^"]+)","from_name","([^"]*)","from_address","([^"]*)","subject","([^"]*)","time",(\d+)/g;
  while ((m = re2.exec(text)) !== null) {
    out.push({
      id: m[1],
      to_address: recipientEmail,
      from_name: m[2],
      from_address: m[3],
      subject: m[4],
      date: parseInt(m[5], 10),
      text: '',
      html: '',
      attachments: [],
    });
  }
  return out;
}

function mergeMailsById(lists: any[][]): any[] {
  const map = new Map<string, any>();
  let anon = 0;
  for (const list of lists) {
    for (const mail of list) {
      let id = String(mail?.id || '');
      if (!id) {
        id = `__smail_${anon}_${mail?.date ?? ''}_${String(mail?.subject ?? '').slice(0, 48)}`;
        anon += 1;
        mail.id = id;
      }
      if (!map.has(id)) {
        map.set(id, mail);
      }
    }
  }
  return [...map.values()];
}

/**
 * 官方 loader 返回 D1 行：{ id, to_address, from_name, from_address, subject, time }（见 akazwz/smail app/types/email.ts）。
 * Flight 序列化后多为普通 JSON 对象，而非 "id","…" 交错字符串元组，须在整棵树递归收集。
 */
function collectPlainRowEmails(root: unknown, recipientEmail: string): any[] {
  const mails: any[] = [];
  const seen = new WeakSet<object>();

  function walk(node: unknown): void {
    if (node === null || node === undefined) {
      return;
    }
    if (typeof node !== 'object') {
      return;
    }
    if (seen.has(node as object)) {
      return;
    }
    seen.add(node as object);

    if (Array.isArray(node)) {
      for (const el of node) {
        walk(el);
      }
      return;
    }

    const o = node as Record<string, unknown>;
    if (typeof o.subject === 'string') {
      const tr = o.time;
      const timeMs =
        typeof tr === 'number' && Number.isFinite(tr)
          ? tr
          : typeof tr === 'string' && /^\d+$/.test(tr)
            ? parseInt(tr, 10)
            : NaN;
      if (Number.isFinite(timeMs)) {
        mails.push({
          id: String(o.id ?? ''),
          to_address: String(o.to_address ?? recipientEmail),
          from_name: String(o.from_name ?? ''),
          from_address: String(o.from_address ?? ''),
          subject: o.subject,
          date: timeMs,
          text: '',
          html: '',
          attachments: [],
        });
      }
    }

    for (const v of Object.values(o)) {
      if (v !== null && typeof v === 'object') {
        walk(v);
      }
    }
  }

  walk(root);
  return mails;
}

/**
 * Flight 风格 payload：根为数组，"addresses",[i] 表示 root[i] 为邮箱字符串；
 * "emails",[a,b,...] 中每项为下标，指向 root[idx] 的邮件行（常为嵌套数组，内含 id/to_address/subject/time 键值序列）。
 */
function resolveFlightSlot(root: unknown[], idx: number, visited: Set<number>): unknown {
  if (idx < 0 || idx >= root.length || visited.has(idx)) {
    return undefined;
  }
  visited.add(idx);
  const val = root[idx];
  if (typeof val === 'number' && Number.isInteger(val) && val >= 0 && val < root.length) {
    return resolveFlightSlot(root, val, visited);
  }
  return val;
}

function flattenFlightIndices(refs: unknown[]): number[] {
  const out: number[] = [];
  for (const r of refs) {
    if (typeof r === 'number' && Number.isInteger(r)) {
      out.push(r);
    } else if (typeof r === 'string' && /^\d+$/.test(r)) {
      out.push(parseInt(r, 10));
    } else if (Array.isArray(r)) {
      for (const x of r) {
        if (typeof x === 'number' && Number.isInteger(x)) {
          out.push(x);
        } else if (typeof x === 'string' && /^\d+$/.test(x)) {
          out.push(parseInt(x, 10));
        }
      }
    }
  }
  return out;
}

function parseSmailEmailsFromFlightRoot(root: unknown[], recipientEmail: string): any[] {
  const mails: any[] = [];
  for (let i = 0; i < root.length - 1; i++) {
    if (root[i] !== 'emails') {
      continue;
    }
    const refs = root[i + 1];
    if (!Array.isArray(refs)) {
      break;
    }
    for (const r of flattenFlightIndices(refs)) {
      const node = resolveFlightSlot(root, r, new Set());
      if (Array.isArray(node)) {
        const leaves = flightLinearLeaves(node);
        mails.push(...parseSmailEmailsFromLinear(leaves, recipientEmail));
      } else if (node !== null && typeof node === 'object') {
        mails.push(...collectPlainRowEmails(node, recipientEmail));
      }
    }
    break;
  }
  return mails;
}

function parseSmailEmailsFromPayload(text: string, recipientEmail: string): any[] {
  const regexMails = parseSmailEmailsRegex(text, recipientEmail);
  let linearMails: any[] = [];
  let flightMails: any[] = [];
  let plainMails: any[] = [];
  try {
    const root = JSON.parse(text) as unknown;
    plainMails = collectPlainRowEmails(root, recipientEmail);
    if (Array.isArray(root)) {
      flightMails = parseSmailEmailsFromFlightRoot(root, recipientEmail);
      const leaves = flightLinearLeaves(root);
      linearMails = parseSmailEmailsFromLinear(leaves, recipientEmail);
    }
  } catch {
    /* 非 JSON 或结构异常时仅用正则 */
  }
  return mergeMailsById([regexMails, linearMails, flightMails, plainMails]);
}

/**
 * POST intent=generate，保存 Set-Cookie __session，从 RSC 响应中解析邮箱
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const response = await fetchWithTimeout(ROOT_DATA_URL, {
    method: 'POST',
    headers: {
      ...DEFAULT_HEADERS,
      'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8',
    },
    body: 'intent=generate',
  });

  if (!response.ok) {
    throw new Error(`smail.pw generate failed: ${response.status}`);
  }

  const cookie = extractSessionCookie(response);
  if (!cookie) {
    throw new Error('Failed to extract __session cookie');
  }

  const text = await response.text();
  const email = extractInboxEmail(text);
  if (!email) {
    throw new Error('Failed to parse inbox address from smail.pw response');
  }

  return {
    channel: CHANNEL,
    email,
    token: cookie,
  };
}

/**
 * GET _root.data，携带 __session Cookie，解析邮件列表
 */
export async function getEmails(token: string, email: string): Promise<Email[]> {
  const response = await fetchWithTimeout(ROOT_DATA_URL, {
    method: 'GET',
    headers: {
      ...DEFAULT_HEADERS,
      Cookie: token,
    },
  });

  if (!response.ok) {
    throw new Error(`smail.pw get emails failed: ${response.status}`);
  }

  const text = await response.text();
  const rawList = parseSmailEmailsFromPayload(text, email);
  return rawList.map((raw) => normalizeEmail(raw, email));
}
