import { readFileSync } from "fs";
import { join } from "path";

let cached: string | null = null;

export function getSdkVersion(): string {
  if (cached !== null) {
    return cached;
  }
  try {
    const pkgPath = join(__dirname, "..", "package.json");
    const raw = readFileSync(pkgPath, "utf8");
    const j = JSON.parse(raw) as { version?: string };
    cached =
      j.version && String(j.version).trim()
        ? String(j.version).trim()
        : "0.0.0";
  } catch {
    cached = "0.0.0";
  }
  return cached;
}
