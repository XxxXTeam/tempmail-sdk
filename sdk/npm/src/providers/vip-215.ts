import WebSocket from 'ws';
import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';
import { fetchWithTimeout } from '../retry';

const CHANNEL: Channel = 'vip-215';
const API_URL = 'https://vip.215.im/api/temp-inbox';
const WS_ORIGIN = 'https://vip.215.im';

/*
 * 推送无正文时，各 SDK 统一合成 text/html（synthetic-v1），便于解析：
 * - text：首行标记 + 每行 key: value
 * - html：根节点 .tempmail-sdk-synthetic[data-tempmail-sdk-format="synthetic-v1"] + dl/dt/dd
 */
const SYNTHETIC_MARKER = '【tempmail-sdk|synthetic|vip-215|v1】';

function sanitizeOneLine(s: unknown): string {
  if (s === undefined || s === null) return '';
  return String(s).replace(/\r\n|\r|\n/g, ' ').trim();
}

function escapeHtml(s: string): string {
  return s
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

function buildSyntheticBodies(
  recipientEmail: string,
  d: { id?: string; from?: string; subject?: string; date?: string; size?: number },
): { text: string; html: string } {
  const id = sanitizeOneLine(d.id);
  const from = sanitizeOneLine(d.from);
  const subject = sanitizeOneLine(d.subject);
  const date = sanitizeOneLine(d.date);
  const to = sanitizeOneLine(recipientEmail);
  const size = typeof d.size === 'number' && Number.isFinite(d.size) ? d.size : undefined;

  const lines = [
    SYNTHETIC_MARKER,
    `id: ${id}`,
    `subject: ${subject}`,
    `from: ${from}`,
    `to: ${to}`,
    `date: ${date}`,
  ];
  if (size !== undefined && size >= 0) {
    lines.push(`size: ${Math.trunc(size)}`);
  }
  const text = lines.join('\n');

  const rows: string[] = [
    ['id', id],
    ['subject', subject],
    ['from', from],
    ['to', to],
    ['date', date],
  ].map(
    ([k, v]) =>
      `<dt>${escapeHtml(k)}</dt><dd>${escapeHtml(v)}</dd>`,
  );
  if (size !== undefined && size >= 0) {
    rows.push(`<dt>size</dt><dd>${escapeHtml(String(Math.trunc(size)))}</dd>`);
  }
  const html =
    `<div class="tempmail-sdk-synthetic" data-tempmail-sdk-format="synthetic-v1" data-channel="vip-215">` +
    `<dl class="tempmail-sdk-meta">${rows.join('')}</dl></div>`;

  return { text, html };
}

const DEFAULT_HEADERS: Record<string, string> = {
  'User-Agent':
    'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
  'Accept-Language': 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
  'Cache-Control': 'no-cache',
  'Content-Type': 'application/json',
  DNT: '1',
  Origin: 'https://vip.215.im',
  Pragma: 'no-cache',
  Referer: 'https://vip.215.im/',
  'Sec-CH-UA': '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
  'Sec-CH-UA-Mobile': '?0',
  'Sec-CH-UA-Platform': '"Windows"',
  'Sec-Fetch-Dest': 'empty',
  'Sec-Fetch-Mode': 'cors',
  'Sec-Fetch-Site': 'same-origin',
  'X-Locale': 'zh',
};

type MailboxState = {
  emails: Map<string, Email>;
  ws: WebSocket | null;
  connectPromise?: Promise<void>;
};

const mailboxes = new Map<string, MailboxState>();

function getState(token: string): MailboxState {
  let st = mailboxes.get(token);
  if (!st) {
    st = { emails: new Map(), ws: null };
    mailboxes.set(token, st);
  }
  return st;
}

function wsUrl(token: string): string {
  return `wss://vip.215.im/v1/ws?token=${encodeURIComponent(token)}`;
}

function ensureWs(token: string, recipientEmail: string): Promise<void> {
  const st = getState(token);
  if (st.ws?.readyState === WebSocket.OPEN) {
    return Promise.resolve();
  }
  if (st.connectPromise) {
    return st.connectPromise;
  }

  st.connectPromise = new Promise<void>((resolve, reject) => {
    const ws = new WebSocket(wsUrl(token), {
      headers: {
        'User-Agent': DEFAULT_HEADERS['User-Agent'],
        Origin: WS_ORIGIN,
      },
    });

    st.ws = ws;

    const clearConnecting = () => {
      st.connectPromise = undefined;
    };

    ws.once('open', () => {
      clearConnecting();
      resolve();
    });

    ws.once('error', (err: Error) => {
      clearConnecting();
      st.ws = null;
      reject(err);
    });

    ws.on('message', (data: WebSocket.RawData) => {
      try {
        const msg = JSON.parse(data.toString()) as {
          type?: string;
          data?: { id?: string; from?: string; subject?: string; date?: string; size?: number };
        };
        if (msg?.type !== 'message.new' || !msg.data) {
          return;
        }
        const d = msg.data;
        const syn = buildSyntheticBodies(recipientEmail, d);
        const raw = {
          id: d.id,
          from: d.from,
          subject: d.subject,
          date: d.date,
          to: recipientEmail,
          text: syn.text,
          html: syn.html,
        };
        const norm = normalizeEmail(raw, recipientEmail);
        if (norm.id) {
          st.emails.set(norm.id, norm);
        }
      } catch {
        /* 忽略非 JSON / 非预期帧 */
      }
    });

    ws.on('close', () => {
      st.ws = null;
      st.connectPromise = undefined;
    });
  });

  return st.connectPromise;
}

/**
 * 创建临时收件箱（服务端分配地址与 JWT，收件靠 WebSocket 推送）
 */
export async function generateEmail(): Promise<InternalEmailInfo> {
  const response = await fetchWithTimeout(API_URL, {
    method: 'POST',
    headers: DEFAULT_HEADERS,
  });

  if (!response.ok) {
    const text = await response.text().catch(() => '');
    throw new Error(`vip-215 create inbox failed: ${response.status} ${text}`);
  }

  const body = (await response.json()) as {
    success?: boolean;
    data?: {
      address?: string;
      token?: string;
      createdAt?: string;
      expiresAt?: string;
    };
  };

  if (!body?.success || !body.data?.address || !body.data?.token) {
    throw new Error('vip-215: invalid API response');
  }

  return {
    channel: CHANNEL,
    email: body.data.address,
    token: body.data.token,
    createdAt: body.data.createdAt,
    expiresAt: body.data.expiresAt,
  };
}

/**
 * 通过 WebSocket 累积 `message.new` 推送；无 MIME 正文时按 synthetic-v1 规范填充 text/html
 */
export async function getEmails(token: string, email: string): Promise<Email[]> {
  await ensureWs(token, email);
  const st = getState(token);
  const list = Array.from(st.emails.values());
  list.sort((a, b) => (a.date || '').localeCompare(b.date || ''));
  return list;
}
