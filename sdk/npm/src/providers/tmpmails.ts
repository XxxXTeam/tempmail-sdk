import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'tmpmails';
const ORIGIN = 'https://tmpmails.com';
const TOKEN_SEP = '\t';

const EMAIL_RE = /[a-zA-Z0-9._-]+@tmpmails\.com/g;
const PAGE_CHUNK_RE = /\/_next\/static\/chunks\/app\/%5Blocale%5D\/page-[a-f0-9]+\.js/;
const INBOX_ACTION_RE = /"([0-9a-f]+)",[^,]+,void 0,[^,]+,"getInboxList"/;

function localeFromDomain(domain?: string | null): string {
  const s = String(domain ?? '').trim();
  return s || 'zh';
}

function setCookieLines(headers: Headers): string[] {
  const h = headers as Headers & { getSetCookie?: () => string[] };
  if (typeof h.getSetCookie === 'function') {
    return h.getSetCookie();
  }
  const one = headers.get('set-cookie');
  return one ? [one] : [];
}

function parseUserSign(headers: Headers): string {
  for (const line of setCookieLines(headers)) {
    const m = /^user_sign=([^;]+)/i.exec(line);
    if (m) {
      const v = m[1].trim();
      if (v) return v;
    }
  }
  throw new Error('tmpmails: missing user_sign cookie');
}

function pickEmail(html: string): string {
  const support = 'support@tmpmails.com';
  const counts = new Map<string, number>();
  for (const m of html.matchAll(EMAIL_RE)) {
    const addr = m[0];
    if (addr.toLowerCase() === support) continue;
    counts.set(addr, (counts.get(addr) ?? 0) + 1);
  }
  if (counts.size === 0) {
    throw new Error('tmpmails: no inbox address in page');
  }
  let best = '';
  let bestC = 0;
  for (const [addr, c] of counts) {
    if (c > bestC || (c === bestC && addr < best)) {
      best = addr;
      bestC = c;
    }
  }
  return best;
}

async function fetchInboxActionID(html: string): Promise<string> {
  const sub = html.match(PAGE_CHUNK_RE)?.[0];
  if (!sub) throw new Error('tmpmails: page chunk script not found');
  const res = await fetchWithTimeout(`${ORIGIN}${sub}`, {
    headers: {
      'User-Agent':
        'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
      Accept: '*/*',
      Referer: `${ORIGIN}/`,
    },
  });
  if (!res.ok) {
    throw new Error(`tmpmails page chunk: ${res.status}`);
  }
  const js = await res.text();
  const m = js.match(INBOX_ACTION_RE);
  if (!m?.[1]) throw new Error('tmpmails: getInboxList action not found in chunk');
  return m[1];
}

export async function generateEmail(domain?: string | null): Promise<InternalEmailInfo> {
  const loc = localeFromDomain(domain);
  const pageUrl = `${ORIGIN}/${loc}`;
  const res = await fetchWithTimeout(pageUrl, {
    headers: {
      'User-Agent':
        'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
      Accept: 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
      'Accept-Language': 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
      'Cache-Control': 'no-cache',
      DNT: '1',
      Pragma: 'no-cache',
      Referer: pageUrl,
      'Upgrade-Insecure-Requests': '1',
    },
  });
  if (!res.ok) {
    throw new Error(`tmpmails generate: ${res.status}`);
  }
  const html = await res.text();
  const userSign = parseUserSign(res.headers);
  const email = pickEmail(html);
  const actionId = await fetchInboxActionID(html);
  const token = `${loc}${TOKEN_SEP}${userSign}${TOKEN_SEP}${actionId}`;
  return { channel: CHANNEL, email, token };
}

function parseInboxResponse(recipient: string, raw: string): Email[] {
  const text = raw.split('"$undefined"').join('null');
  const out: Email[] = [];
  let dataErr: Error | null = null;
  for (const line of text.split('\n')) {
    const trimmed = line.trim();
    if (!trimmed) continue;
    const colon = trimmed.indexOf(':');
    if (colon <= 0) continue;
    const jsonPart = trimmed.slice(colon + 1).trim();
    if (!jsonPart.startsWith('{')) continue;
    let wrap: { code?: number; data?: { list?: unknown[] } };
    try {
      wrap = JSON.parse(jsonPart) as { code?: number; data?: { list?: unknown[] } };
    } catch {
      continue;
    }
    if (wrap.code !== 200 || !wrap.data) continue;
    const list = wrap.data.list;
    if (!Array.isArray(list)) {
      dataErr = new Error('tmpmails: invalid list');
      continue;
    }
    for (const item of list) {
      if (item && typeof item === 'object') {
        out.push(normalizeEmail(item, recipient));
      }
    }
    return out;
  }
  if (dataErr) throw dataErr;
  throw new Error('tmpmails: inbox payload not found');
}

export async function getEmails(email: string, token: string): Promise<Email[]> {
  const parts = token.split(TOKEN_SEP);
  if (parts.length !== 3) {
    throw new Error('tmpmails: invalid session token');
  }
  const [locale, userSign, actionId] = parts;
  const postUrl = `${ORIGIN}/${locale}`;
  const body = JSON.stringify([userSign, email, 0]);
  const res = await fetchWithTimeout(postUrl, {
    method: 'POST',
    headers: {
      'User-Agent':
        'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
      Accept: 'text/x-component',
      'Content-Type': 'text/plain;charset=UTF-8',
      'Next-Action': actionId,
      Origin: ORIGIN,
      Referer: postUrl,
      Cookie: `NEXT_LOCALE=${locale}; user_sign=${userSign}`,
    },
    body,
  });
  if (!res.ok) {
    throw new Error(`tmpmails inbox: ${res.status}`);
  }
  const raw = await res.text();
  return parseInboxResponse(email, raw);
}
