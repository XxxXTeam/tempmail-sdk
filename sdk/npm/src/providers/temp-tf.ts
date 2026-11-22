import { InternalEmailInfo, Email, Channel } from "../types";
import { normalizeEmail } from "../normalize";
import { fetchWithTimeout } from "../retry";

const CHANNEL: Channel = "temp-tf";
const API_BASE = "https://temp.tf/api";

export async function generateEmail(
  domain?: string | null,
  channel: Channel = CHANNEL,
): Promise<InternalEmailInfo> {
  const response = await fetchWithTimeout(`${API_BASE}/account?plus=1`, {
    method: "GET",
    headers: { Accept: "application/json" },
  });
  if (!response.ok) throw new Error(`temp-tf: HTTP ${response.status}`);
  const data = (await response.json()) as { email?: string };
  if (!data.email) throw new Error("temp-tf: empty email");
  return { channel, email: data.email };
}

export async function getEmails(email: string): Promise<Email[]> {
  const address = (email || "").trim();
  if (!address) throw new Error("temp-tf: empty email");

  const response = await fetchWithTimeout(`${API_BASE}/check`, {
    method: "POST",
    headers: { Accept: "application/json", "Content-Type": "application/json" },
    body: JSON.stringify({ email: address }),
  });
  if (!response.ok) throw new Error(`temp-tf: HTTP ${response.status}`);
  const data = (await response.json()) as { data?: any[] };
  const list = Array.isArray(data.data) ? data.data : [];
  return list.map((item) => {
    const isText =
      String(item.bodyContentType || "").toLowerCase() === "text";
    return normalizeEmail(
      {
        id: item.id || "",
        from: item.from || "",
        to: address,
        subject: item.subject || "",
        text: isText ? item.body || "" : "",
        html: isText ? "" : item.body || "",
        date: item.date || "",
        attachments: item.attachments,
      },
      address,
    );
  });
}
