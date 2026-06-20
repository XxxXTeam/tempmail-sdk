import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'mailnesia';
const BASE_URL = 'https://mailnesia.com';
const DOMAIN = 'mailnesia.com';

function randomLocal(): string {
  const chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
  let out = 'sdk';
  for (let i = 0; i < 16; i++) out += chars[Math.floor(Math.random() * chars.length)];
  return out;
}

function decodeHtmlEntities(s: string): string {
  return s
    .replace(/&#x([0-9a-fA-F]+);/g, (_, h) => String.fromCodePoint(parseInt(h, 16)))
    .replace(/&#(\d+);/g, (_, d) => String.fromCodePoint(parseInt(d, 10)))
    .replace(/&nbsp;/g, ' ')
    .replace(/&amp;/g, '&')
    .replace(/&lt;/g, '<')
    .replace(/&gt;/g, '>')
    .replace(/&quot;/g, '"')
    .replace(/&#39;/g, "'");
}

function cleanHtmlText(raw: string): string {
  return decodeHtmlEntities(raw.replace(/<[^>]+>/g, ' ')).replace(/\s+/g, ' ').trim();
}

function localPart(email: string): string {
  return String(email || '').trim().split('@')[0] || '';
}

function mailboxUrl(local: string): string {
  return `${BASE_URL}/mailbox/${encodeURIComponent(local)}`;
}

function detailUrl(local: string, id: string): string {
  return `${mailboxUrl(local)}/${encodeURIComponent(id)}`;
}

async function fetchHtml(url: string): Promise<string> {
  const response = await fetchWithTimeout(url, {
    method: 'GET',
    headers: { Accept: 'text/html,*/*' },
  });
  if (!response.ok) throw new Error(`mailnesia http ${response.status}`);
  return response.text();
}

function parseRows(html: string): Array<Record<string, unknown>> {
  const rows: Array<Record<string, unknown>> = [];
  const rowRe = /<tr\s+id="([^"]+)"[^>]*class="[^"]*\bemailheader\b[^"]*"[^>]*>([\s\S]*?)<\/tr>/gi;
  for (const m of html.matchAll(rowRe)) {
    const id = m[1];
    const row = m[2];
    const date = row.match(/<time\s+datetime="([^"]+)"/i)?.[1] || '';
    const anchors = [...row.matchAll(/<a\b[^>]*class="email"[^>]*>([\s\S]*?)<\/a>/gi)].map(a => cleanHtmlText(a[1]));
    if (anchors.length < 3) continue;
    rows.push({
      id,
      date,
      from: anchors[0],
      to: anchors[1],
      subject: anchors[2],
    });
  }
  return rows;
}

function extractDivById(page: string, id: string, nextId?: string): string {
  const idNeedle = `id="${id}"`;
  const idAt = page.indexOf(idNeedle);
  if (idAt < 0) return '';
  const openEnd = page.indexOf('>', idAt);
  if (openEnd < 0) return '';
  let end = -1;
  if (nextId) {
    end = page.indexOf(`<div id="${nextId}"`, openEnd + 1);
  }
  if (end < 0) {
    end = page.indexOf('</div>', openEnd + 1);
    if (end >= 0) end += '</div>'.length;
  }
  if (end < 0) return '';
  let content = page.slice(openEnd + 1, end).trim();
  if (content.endsWith('</div>')) {
    content = content.slice(0, -'</div>'.length).trim();
  }
  return content;
}

function parsePlain(page: string, id: string): string {
  const re = new RegExp(`<div\\s+id="text_plain_${id}"[^>]*>\\s*<pre>([\\s\\S]*?)<\\/pre>\\s*<\\/div>`, 'i');
  const m = page.match(re);
  return m ? decodeHtmlEntities(m[1]).trim() : '';
}

async function fetchDetail(local: string, row: Record<string, unknown>): Promise<Record<string, unknown>> {
  const id = String(row.id || '');
  if (!id) return row;
  const page = await fetchHtml(detailUrl(local, id));
  const html = extractDivById(page, `text_html_${id}`, `text_plain_${id}`);
  const text = parsePlain(page, id);
  return {
    ...row,
    text,
    html,
  };
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const local = randomLocal();
  await fetchHtml(mailboxUrl(local));
  return { channel: CHANNEL, email: `${local}@${DOMAIN}` };
}

export async function getEmails(email: string): Promise<Email[]> {
  const local = localPart(email);
  if (!local) throw new Error('mailnesia: empty email');
  const html = await fetchHtml(mailboxUrl(local));
  const rows = parseRows(html);
  const emails: Email[] = [];
  for (const row of rows) {
    try {
      emails.push(normalizeEmail(await fetchDetail(local, row), email));
    } catch {
      emails.push(normalizeEmail(row, email));
    }
  }
  return emails;
}
