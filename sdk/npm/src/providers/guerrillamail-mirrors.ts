import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const HEADERS: Record<string, string> = {
  "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
};

function createMirrorProvider(channel: Channel, apiBase: string) {
  async function generateEmail(): Promise<InternalEmailInfo> {
    const res = await fetchWithTimeout(
      `${apiBase}?f=get_email_address&lang=en`,
      { headers: HEADERS },
    );
    if (!res.ok) throw new Error(`${channel}: generate failed: ${res.status}`);
    const data = (await res.json()) as {
      email_addr?: string;
      sid_token?: string;
      email_timestamp?: number;
    };
    if (!data.email_addr || !data.sid_token)
      throw new Error(`${channel}: missing email_addr or sid_token`);
    return {
      channel,
      email: data.email_addr,
      token: data.sid_token,
      expiresAt: data.email_timestamp
        ? (data.email_timestamp + 3600) * 1000
        : undefined,
    };
  }

  async function getEmails(token: string, email: string): Promise<Email[]> {
    const listRes = await fetchWithTimeout(
      `${apiBase}?f=check_email&seq=0&sid_token=${encodeURIComponent(token)}`,
      { headers: HEADERS },
    );
    if (!listRes.ok)
      throw new Error(`${channel}: get emails failed: ${listRes.status}`);
    const data = (await listRes.json()) as { list?: any[] };
    const list = Array.isArray(data.list) ? data.list : [];

    const out: Email[] = [];
    for (const item of list) {
      let body = item.mail_body || "";
      if (!body && item.mail_id) {
        try {
          const dr = await fetchWithTimeout(
            `${apiBase}?f=fetch_email&sid_token=${encodeURIComponent(token)}&email_id=${encodeURIComponent(item.mail_id)}`,
            { headers: HEADERS },
          );
          if (dr.ok) {
            const d = (await dr.json()) as any;
            body = d.mail_body || "";
          }
        } catch {}
      }
      const text = body
        ? body
            .replace(/<[^>]+>/g, " ")
            .replace(/\s+/g, " ")
            .trim()
        : item.mail_excerpt || "";
      out.push(
        normalizeEmail(
          {
            id: item.mail_id,
            from: item.mail_from,
            to: email,
            subject: item.mail_subject,
            text,
            html: body,
            date: item.mail_date || "",
            isRead: item.mail_read === 1,
          },
          email,
        ),
      );
    }
    return out;
  }

  return { generateEmail, getEmails };
}

const sharklasers = createMirrorProvider(
  "sharklasers",
  "https://www.sharklasers.com/ajax.php",
);
const sharklasersCom = createMirrorProvider(
  "sharklasers-com",
  "https://sharklasers.com/ajax.php",
);
const grrla = createMirrorProvider("grr-la", "https://www.grr.la/ajax.php");
const grrlaCom = createMirrorProvider("grr-la-com", "https://grr.la/ajax.php");
const guerrillainfoMirror = createMirrorProvider(
  "guerrillamail-info",
  "https://www.guerrillamail.info/ajax.php",
);
const spam4meMirror = createMirrorProvider(
  "spam4me",
  "https://www.spam4.me/ajax.php",
);
const guerrillamailNetMirror = createMirrorProvider(
  "guerrillamail-net",
  "https://www.guerrillamail.net/ajax.php",
);
const guerrillamailOrgMirror = createMirrorProvider(
  "guerrillamail-org",
  "https://www.guerrillamail.org/ajax.php",
);
const guerrillamailBlockMirror = createMirrorProvider(
  "guerrillamailblock",
  "https://www.guerrillamailblock.com/ajax.php",
);
const guerrillamailComMirror = createMirrorProvider(
  "guerrillamail-com",
  "https://guerrillamail.com/ajax.php",
);
const guerrillamailComWwwMirror = createMirrorProvider(
  "guerrillamail-com-www",
  "https://www.guerrillamail.com/ajax.php",
);

export {
  sharklasers,
  sharklasersCom,
  grrla,
  grrlaCom,
  guerrillainfoMirror,
  spam4meMirror,
  guerrillamailNetMirror,
  guerrillamailOrgMirror,
  guerrillamailBlockMirror,
  guerrillamailComMirror,
  guerrillamailComWwwMirror,
};
