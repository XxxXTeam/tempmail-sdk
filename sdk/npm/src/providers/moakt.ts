import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'moakt';
const ORIGIN = 'https://www.moakt.com';
const TOK_PREFIX = 'mok1:';

const EMAIL_DIV_RE = /<div\s+id="email-address"\s*>([^<]+)<\/div>/is;
const HREF_EMAIL_RE =
  /href="(\/[^"]+\/email\/[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})"/gi;
const TITLE_RE = /<li\s+class="title"\s*>([^<]*)<\/li>/is;
const DATE_RE = /<li\s+class="date"[^>]*>[\s\S]*?<span[^>]*>([^<]+)<\/span>/is;
const SENDER_RE = /<li\s+class="sender"[^>]*>[\s\S]*?<span[^>]*>([\s\S]*?)<\/span>\s*<\/li>/is;
const BODY_RE = /<div\s+class="email-body"\s*>([\s\S]*?)<\/div>/is;
const FROM_ADDR_RE = /<([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})>/;
const TAG_RE = /<[^>]+>/g;

function setCookieLines(headers: Headers): string[] {
  const h = headers as Headers & { getSetCookie?: () => string[] };
  if (typeof h.getSetCookie === 'function') {
    return h.getSetCookie();
  }
  const one = headers.get('set-cookie');
  return one ? [one] : [];
}

function localeFromDomain(domain?: string | null): string {
  const s = String(domain ?? '').trim();
  if (!s || /[/?#\\]/.test(s)) return 'zh';
  return s;
}

function parseCookieMap(hdr: string): Map<string, string> {
  const m = new Map<string, string>();
  for (const part of hdr.split(';')) {
    const p = part.trim();
    if (!p || !p.includes('=')) continue;
    const i = p.indexOf('=');
    const k = p.slice(0, i).trim();
    const v = p.slice(i + 1).trim();
    if (k) m.set(k, v);
  }
  return m;
}

function mergeCookieHeader(prev: string, headers: Headers): string {
  const m = parseCookieMap(prev);
  for (const line of setCookieLines(headers)) {
    const first = line.split(';')[0] ?? '';
    const i = first.indexOf('=');
    if (i <= 0) continue;
    const k = first.slice(0, i).trim();
    const v = first.slice(i + 1).trim();
    if (k) m.set(k, v);
  }
  return [...m.entries()]
    .sort((a, b) => a[0].localeCompare(b[0]))
    .map(([k, v]) => `${k}=${v}`)
    .join('; ');
}

function lightUnescape(s: string): string {
  return s
    .replace(/&amp;/g, '\u0000')
    .replace(/&quot;/g, '"')
    .replace(/&#34;/g, '"')
    .replace(/&#39;/g, "'")
    .replace(/&lt;/g, '<')
    .replace(/&gt;/g, '>')
    .replace(/\u0000/g, '&');
}

function encSess(l: string, c: string): string {
  const raw = JSON.stringify({ l, c });
  return TOK_PREFIX + Buffer.from(raw, 'utf8').toString('base64');
}

function decSess(tok: string): { l: string; c: string } {
  if (!tok.startsWith(TOK_PREFIX)) {
    throw new Error('moakt: invalid session token');
  }
  let raw: string;
  try {
    raw = Buffer.from(tok.slice(TOK_PREFIX.length), 'base64').toString('utf8');
  } catch {
    throw new Error('moakt: invalid session token');
  }
  const o = JSON.parse(raw) as { l?: string; c?: string };
  const l = (o.l ?? '').trim();
  const c = (o.c ?? '').trim();
  if (!l || !c) throw new Error('moakt: invalid session token');
  return { l, c };
}

function pageHeaders(referer: string): Record<string, string> {
  return {
    Accept:
      'text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8',
    'Accept-Language': 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
    'Cache-Control': 'no-cache',
    DNT: '1',
    Pragma: 'no-cache',
    Referer: referer,
    'Upgrade-Insecure-Requests': '1',
    'User-Agent':
      'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
  };
}

function parseInboxEmail(html: string): string {
  const m = EMAIL_DIV_RE.exec(html);
  if (!m?.[1]) throw new Error('moakt: email-address not found');
  const addr = lightUnescape(m[1].trim());
  if (!addr) throw new Error('moakt: empty email-address');
  return addr;
}

function stripTags(s: string): string {
  return s.replace(TAG_RE, ' ').replace(/\s+/g, ' ').trim();
}

function listMailIds(html: string): string[] {
  const seen = new Set<string>();
  const out: string[] = [];
  HREF_EMAIL_RE.lastIndex = 0;
  let cap: RegExpExecArray | null;
  while ((cap = HREF_EMAIL_RE.exec(html)) !== null) {
    const path = cap[1];
    if (path.includes('/delete')) continue;
    const id = path.split('/').pop() ?? '';
    if (id.length === 36 && !seen.has(id)) {
      seen.add(id);
      out.push(id);
    }
  }
  return out;
}

function parseDetail(page: string, id: string, recipient: string): Record<string, unknown> {
  let fromS = '';
  const sm = SENDER_RE.exec(page);
  if (sm?.[1]) {
    const inner = lightUnescape(sm[1]);
    fromS = stripTags(inner);
    const em = FROM_ADDR_RE.exec(inner);
    if (em?.[1]) fromS = em[1].trim();
  }
  let subject = '';
  const tm = TITLE_RE.exec(page);
  if (tm?.[1]) subject = lightUnescape(tm[1].trim());
  let date = '';
  const dm = DATE_RE.exec(page);
  if (dm?.[1]) date = lightUnescape(dm[1].trim());
  let htmlBody = '';
  const bm = BODY_RE.exec(page);
  if (bm?.[1]) htmlBody = bm[1].trim();
  return {
    id,
    to: recipient,
    from: fromS,
    subject,
    date,
    html: htmlBody,
  };
}

export async function generateEmail(domain?: string | null): Promise<InternalEmailInfo> {
  const loc = localeFromDomain(domain);
  const base = `${ORIGIN}/${loc}`;
  const inbox = `${base}/inbox`;

  const r1 = await fetchWithTimeout(base, { headers: pageHeaders(base) });
  if (!r1.ok) throw new Error(`moakt generate: ${r1.status}`);
  let cookieHdr = mergeCookieHeader('', r1.headers);

  const r2 = await fetchWithTimeout(inbox, {
    headers: { ...pageHeaders(base), Cookie: cookieHdr },
  });
  if (!r2.ok) throw new Error(`moakt inbox: ${r2.status}`);
  cookieHdr = mergeCookieHeader(cookieHdr, r2.headers);
  const html = await r2.text();

  const email = parseInboxEmail(html);
  if (!parseCookieMap(cookieHdr).has('tm_session')) {
    throw new Error('moakt: missing tm_session cookie');
  }
  const token = encSess(loc, cookieHdr);
  return { channel: CHANNEL, email, token };
}

export async function getEmails(email: string, token: string): Promise<Email[]> {
  const { l: loc, c: cookieHdr } = decSess(token);
  const inbox = `${ORIGIN}/${loc}/inbox`;
  const baseRef = `${ORIGIN}/${loc}`;

  const r = await fetchWithTimeout(inbox, {
    headers: { ...pageHeaders(baseRef), Cookie: cookieHdr },
  });
  if (!r.ok) throw new Error(`moakt inbox: ${r.status}`);
  const inboxHtml = await r.text();
  const ids = listMailIds(inboxHtml);
  const out: Email[] = [];
  for (const id of ids) {
    const detail = `${ORIGIN}/${loc}/email/${id}/html`;
    const refer = `${ORIGIN}/${loc}/email/${id}`;
    const rd = await fetchWithTimeout(detail, {
      headers: { ...pageHeaders(refer), Cookie: cookieHdr },
    });
    if (!rd.ok) continue;
    const page = await rd.text();
    out.push(normalizeEmail(parseDetail(page, id, email), email));
  }
  return out;
}
