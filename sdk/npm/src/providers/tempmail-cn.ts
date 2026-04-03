import WebSocket from 'ws';
import { InternalEmailInfo, Email, Channel } from '../types';
import { normalizeEmail } from '../normalize';

const CHANNEL: Channel = 'tempmail-cn';
const DEFAULT_HOST = 'tempmail.cn';
const CONNECT_TIMEOUT_MS = 15000;
const HANDSHAKE_WAIT_MS = 1000;
const INITIAL_SYNC_WAIT_MS = 80;
const SOCKET_IO_VERSIONS = [4, 3];

const DEFAULT_HEADERS: Record<string, string> = {
  'User-Agent':
    'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
  'Accept-Language': 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
  'Cache-Control': 'no-cache',
  DNT: '1',
  Pragma: 'no-cache',
};

type BoxState = {
  emails: Email[];
  seenIds: Set<string>;
  ws: WebSocket | null;
  connectPromise?: Promise<void>;
};

const mailboxes = new Map<string, BoxState>();

function getState(email: string): BoxState {
  const key = email.trim().toLowerCase();
  let st = mailboxes.get(key);
  if (!st) {
    st = { emails: [], seenIds: new Set(), ws: null };
    mailboxes.set(key, st);
  }
  return st;
}

function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function normalizeHost(domain?: string | null): string {
  const raw = String(domain || '').trim();
  if (!raw) {
    return DEFAULT_HOST;
  }

  let host = raw;
  if (/^https?:\/\//i.test(host)) {
    try {
      host = new URL(host).host;
    } catch {
      host = raw;
    }
  } else if (host.includes('/')) {
    try {
      host = new URL(`https://${host}`).host;
    } catch {
      host = host.split('/')[0];
    }
  }

  host = host.replace(/^\.+|\.+$/g, '');
  const at = host.lastIndexOf('@');
  if (at >= 0) {
    host = host.slice(at + 1);
  }
  return host || DEFAULT_HOST;
}

function splitAddress(email: string): { local: string; host: string } {
  const trimmed = email.trim();
  const at = trimmed.indexOf('@');
  if (at <= 0 || at === trimmed.length - 1) {
    throw new Error('tempmail-cn: invalid email address');
  }
  return {
    local: trimmed.slice(0, at),
    host: normalizeHost(trimmed.slice(at + 1)),
  };
}

function socketUrl(host: string, eio: number): string {
  return `wss://${host}/socket.io/?EIO=${eio}&transport=websocket`;
}

function socketHeaders(host: string): Record<string, string> {
  return {
    ...DEFAULT_HEADERS,
    Origin: `https://${host}`,
    Referer: `https://${host}/`,
  };
}

function sendEvent(ws: WebSocket, event: string, payload: unknown): void {
  ws.send(`42${JSON.stringify([event, payload])}`);
}

function parseEventPacket(packet: string): { event: string; payload: any } | null {
  if (!packet.startsWith('42')) {
    return null;
  }
  try {
    const decoded = JSON.parse(packet.slice(2));
    if (!Array.isArray(decoded) || typeof decoded[0] !== 'string') {
      return null;
    }
    return { event: decoded[0], payload: decoded[1] };
  } catch {
    return null;
  }
}

function stableMessageId(raw: any, recipientEmail: string): string {
  const headers = raw?.headers || {};
  return String(
    raw?.id ??
      raw?.messageId ??
      headers['message-id'] ??
      headers.messageId ??
      `${headers.from || raw?.from || ''}\n${headers.subject || raw?.subject || ''}\n${headers.date || raw?.date || ''}\n${recipientEmail}`,
  );
}

function flattenMail(raw: any, recipientEmail: string): any {
  const headers = raw?.headers || {};
  return {
    id: stableMessageId(raw, recipientEmail),
    from: headers.from || raw?.from || '',
    to: recipientEmail,
    subject: headers.subject || raw?.subject || '',
    text: raw?.text || raw?.body || '',
    html: raw?.html || '',
    date: headers.date || raw?.date || '',
    isRead: false,
    attachments: Array.isArray(raw?.attachments) ? raw.attachments : [],
  };
}

async function connectSocket(host: string): Promise<WebSocket> {
  let lastError: Error | null = null;

  for (const version of SOCKET_IO_VERSIONS) {
    try {
      return await new Promise<WebSocket>((resolve, reject) => {
        const ws = new WebSocket(socketUrl(host, version), {
          handshakeTimeout: CONNECT_TIMEOUT_MS,
          headers: socketHeaders(host),
        });

        let timer: NodeJS.Timeout | undefined;
        let sentConnect = false;
        let settled = false;

        const cleanup = () => {
          if (timer) {
            clearTimeout(timer);
            timer = undefined;
          }
          ws.off('message', onMessage);
          ws.off('error', onError);
          ws.off('close', onClose);
        };

        const resolveOnce = () => {
          if (settled) return;
          settled = true;
          cleanup();
          resolve(ws);
        };

        const rejectOnce = (err: Error) => {
          if (settled) return;
          settled = true;
          cleanup();
          try {
            ws.close();
          } catch {
            /* ignore close errors */
          }
          reject(err);
        };

        const armHandshakeFallback = () => {
          if (timer) return;
          timer = setTimeout(resolveOnce, HANDSHAKE_WAIT_MS);
        };

        const onMessage = (data: WebSocket.RawData) => {
          const packet = data.toString();
          if (packet === '2') {
            try {
              ws.send('3');
            } catch {
              /* ignore send errors during probing */
            }
            return;
          }
          if (!sentConnect) {
            if (!packet.startsWith('0')) {
              rejectOnce(new Error(`tempmail-cn: unexpected open packet for EIO=${version}`));
              return;
            }
            sentConnect = true;
            try {
              ws.send('40');
            } catch (error) {
              rejectOnce(error instanceof Error ? error : new Error(String(error)));
              return;
            }
            armHandshakeFallback();
            return;
          }
          if (packet.startsWith('40')) {
            resolveOnce();
          }
        };

        const onError = (error: Error) => rejectOnce(error);
        const onClose = () => rejectOnce(new Error(`tempmail-cn: websocket closed during EIO=${version} handshake`));

        ws.on('message', onMessage);
        ws.once('error', onError);
        ws.once('close', onClose);
      });
    } catch (error) {
      lastError = error instanceof Error ? error : new Error(String(error));
    }
  }

  throw lastError || new Error('tempmail-cn: websocket handshake failed');
}

async function requestShortId(host: string): Promise<string> {
  const ws = await connectSocket(host);

  return await new Promise<string>((resolve, reject) => {
    const timer = setTimeout(() => {
      cleanup();
      try {
        ws.close();
      } catch {
        /* ignore close errors */
      }
      reject(new Error('tempmail-cn: timed out waiting for shortid'));
    }, CONNECT_TIMEOUT_MS);

    const cleanup = () => {
      clearTimeout(timer);
      ws.off('message', onMessage);
      ws.off('error', onError);
      ws.off('close', onClose);
    };

    const finish = (value: string) => {
      cleanup();
      try {
        ws.close();
      } catch {
        /* ignore close errors */
      }
      resolve(value);
    };

    const fail = (error: Error) => {
      cleanup();
      try {
        ws.close();
      } catch {
        /* ignore close errors */
      }
      reject(error);
    };

    const onMessage = (data: WebSocket.RawData) => {
      const packet = data.toString();
      if (packet === '2') {
        ws.send('3');
        return;
      }
      const decoded = parseEventPacket(packet);
      if (!decoded || decoded.event !== 'shortid' || typeof decoded.payload !== 'string') {
        return;
      }
      finish(decoded.payload);
    };

    const onError = (error: Error) => fail(error);
    const onClose = () => fail(new Error('tempmail-cn: websocket closed before shortid arrived'));

    ws.on('message', onMessage);
    ws.once('error', onError);
    ws.once('close', onClose);

    sendEvent(ws, 'request shortid', true);
  });
}

async function ensureMailbox(email: string): Promise<void> {
  const st = getState(email);
  if (st.ws?.readyState === WebSocket.OPEN) {
    return;
  }
  if (st.connectPromise) {
    return st.connectPromise;
  }

  const { local, host } = splitAddress(email);

  st.connectPromise = (async () => {
    try {
      const ws = await connectSocket(host);
      st.ws = ws;

      const detach = () => {
        if (st.ws === ws) {
          st.ws = null;
        }
        st.connectPromise = undefined;
      };

      ws.on('message', (data: WebSocket.RawData) => {
        const packet = data.toString();
        if (packet === '2') {
          try {
            ws.send('3');
          } catch {
            /* ignore late pong failures */
          }
          return;
        }

        const decoded = parseEventPacket(packet);
        if (!decoded || decoded.event !== 'mail' || !decoded.payload) {
          return;
        }

        const normalized = normalizeEmail(flattenMail(decoded.payload, email), email);
        if (!normalized.id || st.seenIds.has(normalized.id)) {
          return;
        }

        st.seenIds.add(normalized.id);
        st.emails.push(normalized);
      });

      ws.on('close', detach);
      ws.on('error', detach);

      sendEvent(ws, 'set shortid', local);
      await sleep(INITIAL_SYNC_WAIT_MS);
    } catch (error) {
      st.connectPromise = undefined;
      throw error;
    }
  })();

  return st.connectPromise;
}

export async function generateEmail(domain?: string | null): Promise<InternalEmailInfo> {
  const host = normalizeHost(domain);
  const shortid = await requestShortId(host);
  return {
    channel: CHANNEL,
    email: `${shortid}@${host}`,
  };
}

export async function getEmails(email: string): Promise<Email[]> {
  await ensureMailbox(email);
  const st = getState(email);
  return [...st.emails];
}
