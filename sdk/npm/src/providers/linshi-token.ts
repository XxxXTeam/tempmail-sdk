import { createHash, randomBytes, randomInt } from 'crypto';
export function deriveLinshiApiPathKey(visitorId: string): string {
  if (visitorId.length < 4) {
    throw new Error('visitorId 过短，无法套用 u()');
  }
  let e = visitorId.substring(0, visitorId.length - 2);
  let t = 0;
  for (let i = 0; i < e.length; i++) {
    t += e.charCodeAt(i) % 5;
  }
  if (t < 10) {
    t = 10;
  }
  if (t >= 100) {
    t = 99;
  }
  const ts = t.toString();
  e = e.slice(0, 3) + ts[0] + e.slice(3);
  e = e.slice(0, 12) + ts[1] + e.slice(12);
  return e;
}
export interface SyntheticBrowserProfile {
  userAgent: string;
  platform: string;
  language: string;
  languages: string;
  hardwareConcurrency: number;
  deviceMemory: number;
  timezone: string;
  colorDepth: number;
  pixelRatio: number;
  screen: { width: number; height: number; availWidth: number; availHeight: number };
}

const UA_POOL = [
  'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36',
  'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/121.0.0.0 Safari/537.36 Edg/121.0.0.0',
  'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36',
  'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/119.0.0.0 Safari/537.36',
  'Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:122.0) Gecko/20100101 Firefox/122.0',
];

function pick<T>(arr: T[]): T {
  return arr[randomInt(arr.length)];
}

export function randomBrowserLikeProfile(): SyntheticBrowserProfile {
  const w = pick([1920, 2560, 1680, 1536, 1366]) + randomInt(0, 80);
  const h = pick([1080, 1440, 1050, 864, 768]) + randomInt(0, 40);
  return {
    userAgent: pick(UA_POOL),
    platform: pick(['Win32', 'MacIntel', 'Linux x86_64']),
    language: pick(['zh-CN', 'en-US', 'en-GB', 'ja-JP']),
    languages: pick(['zh-CN,zh,en', 'en-US,en', 'ja-JP,ja,en']),
    hardwareConcurrency: randomInt(4, 33),
    deviceMemory: pick([4, 8, 16]),
    timezone: pick(['Asia/Shanghai', 'America/New_York', 'Europe/London', 'UTC']),
    colorDepth: pick([24, 30]),
    pixelRatio: pick([1, 1.25, 1.5, 2]),
    screen: {
      width: w,
      height: h,
      availWidth: w,
      availHeight: h - randomInt(24, 120),
    },
  };
}

export function syntheticVisitorIdFromProfile(
  profile: SyntheticBrowserProfile,
  salt: Buffer = randomBytes(16),
): string {
  const payload = JSON.stringify(profile, Object.keys(profile).sort());
  return createHash('sha256').update(payload).update(salt).digest('hex').slice(0, 32);
}

export function randomSyntheticLinshiKey(): {
  profile: SyntheticBrowserProfile;
  visitorId: string;
  apiPathKey: string;
} {
  const profile = randomBrowserLikeProfile();
  const visitorId = syntheticVisitorIdFromProfile(profile);
  const apiPathKey = deriveLinshiApiPathKey(visitorId);
  return { profile, visitorId, apiPathKey };
}
