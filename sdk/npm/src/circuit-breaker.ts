import { logger } from "./logger";

/**
 * 熔断器状态
 * 记录后端最近失败的时间戳，在冷却期内跳过该后端的所有渠道
 */
interface CircuitState {
  /** 最近一次失败时间 (ms) */
  lastFailure: number;
  /** 连续失败次数 */
  failCount: number;
}

const DEFAULT_COOLDOWN_MS = 60_000;
const MAX_COOLDOWN_MS = 300_000;

const circuitMap = new Map<string, CircuitState>();

/**
 * 检查后端是否处于熔断状态
 */
export function isBackendOpen(backend: string): boolean {
  const state = circuitMap.get(backend);
  if (!state) return true;

  const cooldown = Math.min(
    DEFAULT_COOLDOWN_MS * Math.pow(2, state.failCount - 1),
    MAX_COOLDOWN_MS,
  );
  const elapsed = Date.now() - state.lastFailure;

  if (elapsed >= cooldown) {
    circuitMap.delete(backend);
    return true;
  }

  return false;
}

/**
 * 记录后端失败
 */
export function recordBackendFailure(backend: string): void {
  const state = circuitMap.get(backend);
  if (state) {
    state.lastFailure = Date.now();
    state.failCount += 1;
  } else {
    circuitMap.set(backend, { lastFailure: Date.now(), failCount: 1 });
  }
  logger.debug(`后端 ${backend} 熔断，连续失败 ${circuitMap.get(backend)!.failCount} 次`);
}

/**
 * 记录后端成功，重置熔断状态
 */
export function recordBackendSuccess(backend: string): void {
  if (circuitMap.has(backend)) {
    circuitMap.delete(backend);
  }
}

/**
 * 重置所有熔断状态（用于测试）
 */
export function resetAllCircuits(): void {
  circuitMap.clear();
}
