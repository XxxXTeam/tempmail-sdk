import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = '10minute-one';
const SITE_ORIGIN = 'https://10minutemail.one';
const API_BASE = 'https://web.10minutemail.one/api/v1';

const JWT_RE = /^[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+$/;

const DEFAULT_PAGE_HEADERS: Record<string, string> = {
  Accept: 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
  'Accept-Language': 'zh-CN,zh;q=0.9,en;q=0.8',
  'Cache-Control': 'no-cache',
  Pragma: 'no-cache',
  DNT: '1',
  'User-Agent':
    'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36',
};

function extractNuxtDataArray(html: string): unknown[] {
  const m = html.match(/<script[^>]*\bid="__NUXT_DATA__"[^>]*>([\s\S]*?)<\/script>/i);
  if (!m) throw new Error('10minute-one: __NUXT_DATA__ not found');
  return JSON.parse(m[1].trim()) as unknown[];
}

function resolveRef(arr: unknown[], value: unknown, depth = 0): unknown {
  if (depth > 64) throw new Error('10minute-one: resolveRef too deep');
  if (value == null) return value;
  if (typeof value === 'number' && Number.isInteger(value) && value >= 0) {
    if (value >= arr.length) return undefined;
    return resolveRef(arr, arr[value], depth + 1);
  }
  return value;
}

function parseMailServiceTokenFromPayload(arr: unknown[]): string {
  if (!Array.isArray(arr)) throw new Error('10minute-one: invalid __NUXT_DATA__');

  for (let i = 0; i < Math.min(arr.length, 48); i++) {
    const el = arr[i];
    if (el && typeof el === 'object' && !Array.isArray(el) && 'mailServiceToken' in el) {
      const t = resolveRef(arr, (el as Record<string, unknown>).mailServiceToken);
      if (typeof t === 'string' && JWT_RE.test(t)) return t;
    }
  }
  for (const el of arr) {
    if (el && typeof el === 'object' && !Array.isArray(el) && 'mailServiceToken' in el) {
      const t = resolveRef(arr, (el as Record<string, unknown>).mailServiceToken);
      if (typeof t === 'string' && JWT_RE.test(t)) return t;
    }
  }
  for (const el of arr) {
    if (typeof el === 'string' && JWT_RE.test(el)) return el;
  }
  throw new Error('10minute-one: mailServiceToken not found');
}

function parseQuotedJsonArrayField(html: string, field: string): string[] {
  const key = `${field}:"`;
  const start = html.indexOf(key);
  if (start < 0) return [];
  const sliceStart = start + key.length;
  if (html[sliceStart] !== '[') return [];
  let depth = 0;
  let j = sliceStart;
  for (; j < html.length; j++) {
    const c = html[j];
    if (c === '[') depth++;
    else if (c === ']') {
      depth--;
      if (depth === 0) {
        j++;
        break;
      }
    }
  }
  const raw = html.slice(sliceStart, j);
  const unesc = raw.replace(/\\"/g, '"');
  try {
    const v = JSON.parse(unesc);
    return Array.isArray(v) ? v.map(x => String(x)) : [];
  } catch {
    return [];
  }
}

function parseEmailDomains(html: string): string[] {
  const d = parseQuotedJsonArrayField(html, 'emailDomains');
  return d.filter(Boolean);
}

function parseBlockedUsernames(html: string): Set<string> {
  const list = parseQuotedJsonArrayField(html, 'blockedUsernames');
  return new Set(list.map(s => s.toLowerCase()));
}

function randomRequestId(): string {
  const a = new Uint8Array(16);
  crypto.getRandomValues(a);
  return [...a].map(b => b.toString(16).padStart(2, '0')).join('');
}

function b64DecodeUtf8(b64: string): string {
  if (typeof Buffer !== 'undefined') {
    return Buffer.from(b64, 'base64').toString('utf8');
  }
  const bin = atob(b64);
  const bytes = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
  return new TextDecoder('utf-8').decode(bytes);
}

function b64UrlPayload(jwt: string): Record<string, unknown> | null {
  const parts = jwt.split('.');
  if (parts.length < 2) return null;
  let b = parts[1].replace(/-/g, '+').replace(/_/g, '/');
  const pad = b.length % 4 ? '='.repeat(4 - (b.length % 4)) : '';
  b += pad;
  try {
    return JSON.parse(b64DecodeUtf8(b)) as Record<string, unknown>;
  } catch {
    return null;
  }
}

function jwtExpMs(jwt: string): number | undefined {
  const p = b64UrlPayload(jwt);
  const exp = p?.exp;
  if (typeof exp === 'number' && Number.isFinite(exp)) return Math.floor(exp * 1000);
  return undefined;
}

function randomLocal(len: number): string {
  const chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
  const a = new Uint8Array(len);
  crypto.getRandomValues(a);
  let s = '';
  for (let i = 0; i < len; i++) s += chars[a[i] % chars.length];
  return s;
}

function pickLocale(domain?: string | null): string {
  const s = String(domain ?? '').trim();
  if (!s) return 'zh';
  if (s.includes('.') && !s.includes('/')) return 'zh';
  return s;
}

function pickMailboxDomain(domainHint: string | undefined, available: string[]): string {
  if (!available.length) throw new Error('10minute-one: no email domains in page');
  const h = (domainHint ?? '').trim().toLowerCase();
  if (h.includes('.')) {
    for (const d of available) {
      if (d.toLowerCase() === h) return d;
    }
  }
  return available[Math.floor(Math.random() * available.length)]!;
}

function apiHeaders(token: string, extra?: Record<string, string>): Record<string, string> {
  const ts = String(Math.floor(Date.now() / 1000));
  return {
    Accept: '*/*',
    'Accept-Language': DEFAULT_PAGE_HEADERS['Accept-Language']!,
    Authorization: `Bearer ${token}`,
    'Cache-Control': 'no-cache',
    'Content-Type': 'application/json',
    DNT: '1',
    Origin: SITE_ORIGIN,
    Pragma: 'no-cache',
    Referer: `${SITE_ORIGIN}/`,
    'Sec-Fetch-Dest': 'empty',
    'Sec-Fetch-Mode': 'cors',
    'Sec-Fetch-Site': 'same-site',
    'User-Agent': DEFAULT_PAGE_HEADERS['User-Agent']!,
    'X-Request-ID': randomRequestId(),
    'X-Timestamp': ts,
    ...extra,
  };
}

function encMailboxPath(email: string): string {
  return encodeURIComponent(email);
}

function itemNeedsDetail(item: Record<string, unknown>): boolean {
  const id = item.id;
  if (id == null || String(id).trim() === '') return false;
  const subj = String(item.subject ?? item.mail_title ?? '').trim();
  const body = String(item.text ?? item.body ?? item.html ?? item.mail_text ?? '').trim();
  return subj === '' && body === '';
}

export async function generateEmail(domain?: string | null): Promise<InternalEmailInfo> {
  const locale = pickLocale(domain ?? undefined);
  const pageUrl = `${SITE_ORIGIN}/${encodeURIComponent(locale)}`;
  const res = await fetchWithTimeout(pageUrl, { headers: { ...DEFAULT_PAGE_HEADERS, Referer: pageUrl } });
  if (!res.ok) throw new Error(`10minute-one generate: ${res.status}`);
  const html = await res.text();
  const arr = extractNuxtDataArray(html);
  const token = parseMailServiceTokenFromPayload(arr);
  let domains = parseEmailDomains(html);
  if (domains.length === 0) {
    domains = ['xghff.com', 'oqqaj.com', 'psovv.com'];
  }
  const blocked = parseBlockedUsernames(html);
  const domainHint = (domain ?? '').trim().includes('.') ? (domain ?? '').trim() : undefined;
  const mailDomain = pickMailboxDomain(domainHint, domains);

  let local = '';
  for (let attempt = 0; attempt < 32; attempt++) {
    const cand = randomLocal(10);
    if (!blocked.has(cand.toLowerCase())) {
      local = cand;
      break;
    }
  }
  if (!local) throw new Error('10minute-one: could not pick username');

  const address = `${local}@${mailDomain}`;
  const expMs = jwtExpMs(token);
  return {
    channel: CHANNEL,
    email: address,
    token,
    expiresAt: expMs,
  };
}

export async function getEmails(email: string, token: string): Promise<Email[]> {
  const url = `${API_BASE}/mailbox/${encMailboxPath(email)}`;
  const res = await fetchWithTimeout(url, { headers: apiHeaders(token) });
  if (!res.ok) throw new Error(`10minute-one inbox: ${res.status}`);
  const list = (await res.json()) as unknown;
  if (!Array.isArray(list)) throw new Error('10minute-one: invalid inbox JSON');

  const out: Email[] = [];
  for (const raw of list) {
    if (!raw || typeof raw !== 'object') continue;
    let row = raw as Record<string, unknown>;
    if (itemNeedsDetail(row)) {
      const id = encodeURIComponent(String(row.id));
      const du = `${API_BASE}/mailbox/${encMailboxPath(email)}/${id}`;
      const dr = await fetchWithTimeout(du, { headers: apiHeaders(token) });
      if (dr.ok) {
        try {
          const detail = (await dr.json()) as Record<string, unknown>;
          row = { ...row, ...detail };
        } catch {
          /* keep row */
        }
      }
    }
    out.push(normalizeEmail(row, email));
  }
  return out;
}
