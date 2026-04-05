import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = '10mail-wangtz';
const ORIGIN = 'https://10mail.wangtz.cn';
const MAIL_DOMAIN = 'wangtz.cn';

const JSON_HEADERS: Record<string, string> = {
  Accept: '*/*',
  'Accept-Language': 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
  'Content-Type': 'application/json; charset=utf-8',
  Origin: ORIGIN,
  Referer: `${ORIGIN}/`,
  token: 'null',
  Authorization: 'null',
};

function localPartFromDomainOption(domain?: string | null): string {
  const s = String(domain ?? '').trim();
  if (!s) return '';
  const at = s.indexOf('@');
  return at >= 0 ? s.slice(0, at).trim() : s;
}

/** emailList 返回 mailparser 风格：{ text?, value?: [{ address, name }] } */
function formatAddrField(v: unknown): string {
  if (!v || typeof v !== 'object') return '';
  const o = v as { text?: unknown; value?: unknown };
  const text = o.text;
  if (text != null && String(text).trim()) return String(text);
  const value = o.value;
  if (!Array.isArray(value) || value.length === 0) return '';
  const first = value[0];
  if (!first || typeof first !== 'object') return '';
  const addr = String((first as { address?: string }).address ?? '').trim();
  if (!addr) return '';
  const name = String((first as { name?: string }).name ?? '').trim();
  if (name && name.toLowerCase() !== addr.toLowerCase()) return `${name} <${addr}>`;
  return addr;
}

function flattenMessage(raw: Record<string, unknown>): Record<string, unknown> {
  const out: Record<string, unknown> = { ...raw };
  const mid = raw.messageId;
  if (mid != null) out.id = mid;
  const fromStr = formatAddrField(raw.from);
  if (fromStr) out.from = fromStr;
  const toStr = formatAddrField(raw.to);
  if (toStr) out.to = toStr;
  return out;
}

interface TempMailResponse {
  code?: number;
  msg?: string;
  data?: { mailName?: string; endTime?: number };
}

export async function generateEmail(domain?: string | null): Promise<InternalEmailInfo> {
  const emailName = localPartFromDomainOption(domain ?? null);
  const res = await fetchWithTimeout(
    `${ORIGIN}/api/tempMail`,
    {
      method: 'POST',
      headers: {
        ...JSON_HEADERS,
        'User-Agent':
          'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
      },
      body: JSON.stringify({ emailName }),
    },
    undefined,
    { skipTlsVerify: true },
  );
  if (!res.ok) {
    throw new Error(`10mail-wangtz generate: ${res.status}`);
  }
  const parsed = (await res.json()) as TempMailResponse;
  if (parsed.code !== 0 || !parsed.data?.mailName) {
    throw new Error(`10mail-wangtz: ${String(parsed.msg ?? 'create failed')}`);
  }
  const local = String(parsed.data.mailName).trim();
  if (!local) {
    throw new Error('10mail-wangtz: empty mailName');
  }
  const email = `${local}@${MAIL_DOMAIN}`;
  let expiresAt: string | undefined;
  if (parsed.data.endTime != null && Number.isFinite(parsed.data.endTime)) {
    expiresAt = new Date(parsed.data.endTime).toISOString();
  }
  return {
    channel: CHANNEL,
    email,
    token: '',
    expiresAt,
  };
}

export async function getEmails(email: string): Promise<Email[]> {
  const res = await fetchWithTimeout(
    `${ORIGIN}/api/emailList`,
    {
      method: 'POST',
      headers: {
        ...JSON_HEADERS,
        'User-Agent':
          'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
      },
      body: JSON.stringify({ emailName: email.trim() }),
    },
    undefined,
    { skipTlsVerify: true },
  );
  if (!res.ok) {
    throw new Error(`10mail-wangtz inbox: ${res.status}`);
  }
  const list = (await res.json()) as unknown;
  if (!Array.isArray(list)) {
    throw new Error('10mail-wangtz: inbox response is not an array');
  }
  return list.map(item => {
    if (!item || typeof item !== 'object') {
      return normalizeEmail({}, email);
    }
    return normalizeEmail(flattenMessage(item as Record<string, unknown>), email);
  });
}
