import { getConfig } from "./config";
import { getSdkVersion } from "./version";

const DEFAULT_URL = "https://sdk-1.openel.top/v1/event";
const MAX_BATCH = 32;
const FLUSH_MS = 2000;

const EMAIL_LIKE = /[^\s@]{1,64}@[^\s@]{1,255}/g;

interface TelemetryEvent {
  operation: string;
  channel: string;
  success: boolean;
  attempt_count: number;
  channels_tried?: number;
  error?: string;
  ts_ms: number;
}

interface TelemetryBatch {
  schema_version: number;
  sdk_language: string;
  sdk_version: string;
  os: string;
  arch: string;
  events: TelemetryEvent[];
}

const queue: TelemetryEvent[] = [];
let flushTimer: ReturnType<typeof setTimeout> | null = null;
let periodicStarted = false;

function sanitizeError(msg: string): string {
  if (!msg) return "";
  return msg.replace(EMAIL_LIKE, "[redacted]").slice(0, 400);
}

function resolveUrl(): string {
  const c = getConfig();
  const u = (c.telemetryUrl || "").trim();
  if (u) return u;
  return DEFAULT_URL;
}

function telemetryOn(): boolean {
  const v = getConfig().telemetryEnabled;
  if (v === false) return false;
  return true;
}

function getFetchForTelemetry(): { fetchFn: typeof fetch } {
  const c = getConfig();
  return { fetchFn: c.customFetch || fetch };
}

function startPeriodicFlush(): void {
  if (periodicStarted) return;
  periodicStarted = true;
  const timer = setInterval(() => {
    void flushQueue();
  }, FLUSH_MS);
  /* Node 下解除对事件循环的引用，避免仅因遥测定时器阻止进程退出 */
  (timer as { unref?: () => void }).unref?.();
}

function scheduleDebouncedFlush(): void {
  if (flushTimer) clearTimeout(flushTimer);
  flushTimer = setTimeout(() => {
    flushTimer = null;
    void flushQueue();
  }, FLUSH_MS);
}

async function flushQueue(): Promise<void> {
  if (!telemetryOn()) {
    queue.length = 0;
    return;
  }
  if (queue.length === 0) return;

  const events = queue.splice(0, queue.length);
  const url = resolveUrl();
  if (!url) return;

  const sdkVersion = getSdkVersion();
  const batch: TelemetryBatch = {
    schema_version: 2,
    sdk_language: "node",
    sdk_version: sdkVersion,
    os: typeof process !== "undefined" ? process.platform : "unknown",
    arch: typeof process !== "undefined" ? process.arch : "unknown",
    events,
  };

  const { fetchFn } = getFetchForTelemetry();
  try {
    await fetchFn(url, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        "User-Agent": `tempmail-sdk-node/${sdkVersion}`,
      },
      body: JSON.stringify(batch),
    });
  } catch {
    /* ignore */
  }
}

export function reportTelemetry(
  operation: string,
  channel: string,
  success: boolean,
  attemptCount: number,
  channelsTried: number,
  error: string,
): void {
  if (!telemetryOn()) return;

  startPeriodicFlush();

  const ev: TelemetryEvent = {
    operation,
    channel,
    success,
    attempt_count: attemptCount,
    ts_ms: Date.now(),
  };
  if (channelsTried > 0) ev.channels_tried = channelsTried;
  const err = sanitizeError(error);
  if (err) ev.error = err;

  queue.push(ev);

  if (queue.length >= MAX_BATCH) {
    void flushQueue();
  } else {
    scheduleDebouncedFlush();
  }
}
