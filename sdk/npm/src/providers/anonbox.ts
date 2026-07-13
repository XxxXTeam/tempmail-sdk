import { Channel, Email, InternalEmailInfo } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "anonbox";
const PAGE_URL = "https://anonbox.net/en/";
const BASE_URL = "https://anonbox.net";

const MAIL_LINK_RE =
  /<a href="https:\/\/anonbox\.net\/([a-z0-9-]+)\/([^"]+)">https:\/\/anonbox\.net\/[^"]+<\/a>/i;
const DD_RE = /<dd([^>]*)>([\s\S]*?)<\/dd>/gis;
const DISPLAY_NONE_RE = /display\s*:\s*none/i;
const P_RE = /<p>([\s\S]*?)<\/p>/is;
const SPAN_RE = /<span\b[^>]*>[\s\S]*?<\/span>/gis;
const TAG_RE = /<[^>]+>/g;
const EXPIRES_RE =
  /Your mail address is valid until:<\/dt>\s*<dd><p>([^<]+)<\/p>/is;

function stripTags(html: string): string {
  return html
    .replace(TAG_RE, "")
    .replace(/&nbsp;/g, " ")
    .replace(/&amp;/g, "&")
    .replace(/&lt;/g, "<")
    .replace(/&gt;/g, ">")
    .trim();
}

function simpleHash(input: string): string {
  let h = 0;
  for (let i = 0; i < input.length; i++) {
    h = (h * 31 + input.charCodeAt(i)) >>> 0;
  }
  if (h === 0) return "0";
  return h.toString(36);
}

function parseEnPage(html: string): {
  email: string;
  token: string;
  expiresAt?: string;
} {
  const link = MAIL_LINK_RE.exec(html);
  if (!link) {
    throw new Error("anonbox: mailbox link not found");
  }
  const inbox = link[1];
  const secret = link[2];
  const token = `${inbox}/${secret}`;

  let addressHtml = "";
  for (const match of html.matchAll(DD_RE)) {
    const attrs = match[1] || "";
    const inner = match[2] || "";
    if (DISPLAY_NONE_RE.test(attrs)) continue;
    const p = P_RE.exec(inner);
    if (!p) continue;
    const pInner = (p[1] || "").replace(SPAN_RE, "");
    const display = stripTags(pInner);
    if (!display.includes("@")) continue;
    const at = display.lastIndexOf("@");
    const local = display.slice(0, at).trim();
    const domain = display
      .slice(at + 1)
      .trim()
      .toLowerCase();
    if (!local || domain === "googlemail.com") continue;
    if (domain !== `${inbox}.anonbox.net`.toLowerCase()) continue;
    addressHtml = display;
    break;
  }

  if (!addressHtml) {
    throw new Error("anonbox: address paragraph not found");
  }
  const at = addressHtml.indexOf("@");
  if (at < 0) {
    throw new Error("anonbox: bad address");
  }
  const local = addressHtml.slice(0, at).trim();
  if (!local) {
    throw new Error("anonbox: empty local part");
  }

  const expiresAt = EXPIRES_RE.exec(html)?.[1]?.trim();
  return {
    email: `${local}@${inbox}.anonbox.net`,
    token,
    expiresAt: expiresAt || undefined,
  };
}

function decodeQuotedPrintable(body: string, headerBlock: string): string {
  const match = /^content-transfer-encoding:\s*([^\s]+)/im.exec(headerBlock);
  const encoding = match?.[1]?.trim().toLowerCase() || "";
  if (encoding !== "quoted-printable") {
    return body.replace(/\r?\n+$/, "");
  }

  const soft = body.replace(/=\r?\n/g, "");
  const bytes: number[] = [];
  for (let i = 0; i < soft.length; i++) {
    const ch = soft[i];
    if (ch === "=" && i + 2 < soft.length) {
      const hex = soft.slice(i + 1, i + 3);
      if (/^[0-9a-f]{2}$/i.test(hex)) {
        bytes.push(parseInt(hex, 16));
        i += 2;
        continue;
      }
    }
    bytes.push(soft.charCodeAt(i) & 0xff);
  }
  return new TextDecoder("utf-8")
    .decode(new Uint8Array(bytes))
    .replace(/\r?\n+$/, "");
}

function parseHeaders(lines: string[]): {
  headers: Record<string, string>;
  bodyIndex: number;
} {
  const headers: Record<string, string> = {};
  let currentKey = "";
  let i = 0;
  for (; i < lines.length; i++) {
    const line = lines[i];
    if (line === "") {
      i++;
      break;
    }
    if ((line.startsWith(" ") || line.startsWith("\t")) && currentKey) {
      headers[currentKey] = `${headers[currentKey]} ${line.trim()}`;
      continue;
    }
    const idx = line.indexOf(":");
    if (idx > 0) {
      currentKey = line.slice(0, idx).trim().toLowerCase();
      headers[currentKey] = line.slice(idx + 1).trim();
    }
  }
  return { headers, bodyIndex: i };
}

function mboxBlockToRaw(
  block: string,
  recipient: string,
): Record<string, unknown> {
  const normalized = block.replace(/\r\n/g, "\n");
  const lines = normalized.split("\n");
  let bodyStart = 0;
  if (lines.length > 0 && lines[0].startsWith("From ")) {
    bodyStart = 1;
  }
  const parsed = parseHeaders(lines.slice(bodyStart));
  const headers = parsed.headers;
  const body = lines.slice(bodyStart + parsed.bodyIndex).join("\n");
  const contentType = (headers["content-type"] || "").toLowerCase();
  let text = "";
  let html = "";

  if (contentType.includes("multipart/")) {
    const boundaryMatch = /boundary="?([^";\s]+)"?/i.exec(
      headers["content-type"] || "",
    );
    if (boundaryMatch) {
      const boundary = boundaryMatch[1];
      const parts = `\n${body.replace(/\r\n/g, "\n")}`.split(
        new RegExp(
          `\n--${boundary.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")}(?:--)?\n`,
        ),
      );
      for (const partRaw of parts) {
        const part = partRaw.trim();
        if (!part || part === "--") continue;
        const sep = part.indexOf("\n\n");
        if (sep < 0) continue;
        const ph = part.slice(0, sep);
        const pb = part.slice(sep + 2);
        const pct =
          /^content-type:\s*([^\s;]+)/im.exec(ph)?.[1]?.trim().toLowerCase() ||
          "";
        if (pct === "text/plain") {
          text = decodeQuotedPrintable(pb, ph);
        } else if (pct === "text/html") {
          html = decodeQuotedPrintable(pb, ph);
        }
      }
    }
  }

  if (!text && !html) {
    if (contentType.includes("text/html")) {
      html = decodeQuotedPrintable(body, headers["content-type"] || "");
    } else {
      text = decodeQuotedPrintable(body, headers["content-type"] || "");
    }
  }

  let date = headers.date || "";
  if (date) {
    const parsedDate = new Date(date);
    if (!Number.isNaN(parsedDate.getTime())) {
      date = parsedDate.toISOString();
    }
  }
  if (!date) {
    date = new Date().toISOString();
  }

  let id = headers["message-id"] || "";
  if (!id) {
    id = simpleHash(block.slice(0, 512));
  }

  return {
    id,
    from: headers.from || "",
    to: headers.to || recipient,
    subject: headers.subject || "",
    body_text: text,
    body_html: html,
    date,
    isRead: false,
    attachments: [],
  };
}

function splitMbox(raw: string): string[] {
  const s = raw.replace(/\r\n/g, "\n").trim();
  if (!s) return [];
  const blocks: string[] = [];
  let rest = s;
  const sep = "\nFrom ";
  for (;;) {
    const idx = rest.indexOf(sep);
    if (idx < 0) {
      blocks.push(rest);
      break;
    }
    blocks.push(rest.slice(0, idx));
    rest = rest.slice(idx + 1);
  }
  return blocks;
}

export async function generateEmail(): Promise<InternalEmailInfo> {
  const response = await fetchWithTimeout(PAGE_URL, {
    headers: {
      Accept: "text/html,application/xhtml+xml",
      "User-Agent":
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    },
  });
  if (!response.ok) {
    throw new Error(`anonbox: generate HTTP ${response.status}`);
  }
  const html = await response.text();
  const { email, token, expiresAt } = parseEnPage(html);
  return {
    channel: CHANNEL,
    email,
    token,
    expiresAt,
  };
}

export async function getEmails(
  token: string,
  email: string,
): Promise<Email[]> {
  const t = String(token || "").trim();
  if (!t) {
    throw new Error("anonbox: token is required");
  }
  const path = t.endsWith("/") ? t : `${t}/`;
  const response = await fetchWithTimeout(`${BASE_URL}/${path}`, {
    headers: {
      Accept: "text/plain,text/html,*/*",
      "User-Agent":
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    },
  });
  if (!response.ok) {
    throw new Error(`anonbox: inbox HTTP ${response.status}`);
  }
  const raw = await response.text();
  const emails: Email[] = [];
  for (const block of splitMbox(raw)) {
    emails.push(normalizeEmail(mboxBlockToRaw(block, email), email));
  }
  return emails;
}
