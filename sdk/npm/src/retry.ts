/**
 * 通用重试工具
 * 提供请求重试、超时控制、指数退避等错误恢复机制
 */

import { logger } from './logger';
import { getConfig } from './config';

/**
 * 重试配置选项
 */
export interface RetryOptions {
  /** 最大重试次数（不含首次请求），默认 2 */
  maxRetries?: number;
  /** 初始重试延迟（毫秒），默认 1000 */
  initialDelay?: number;
  /** 最大重试延迟（毫秒），默认 5000 */
  maxDelay?: number;
  /** 请求超时时间（毫秒），默认 15000 */
  timeout?: number;
  /** 是否对该错误进行重试的判断函数 */
  shouldRetry?: (error: any) => boolean;
}

const DEFAULT_RETRY_OPTIONS: Required<RetryOptions> = {
  maxRetries: 2,
  initialDelay: 1000,
  maxDelay: 5000,
  timeout: 15000,
  shouldRetry: defaultShouldRetry,
};

/**
 * 默认重试判断
 * 以下错误类型会触发重试：
 * - 网络连接错误（fetch failed, ECONNREFUSED, ECONNRESET 等）
 * - 超时错误（timeout, abort）
 * - DNS 解析失败
 * - HTTP 429 限流
 * - HTTP 4xx/5xx 服务端错误（含状态码的错误消息）
 * 仅 SDK 内部的参数校验类错误不重试
 */
function defaultShouldRetry(error: any): boolean {
  if (!error) return false;

  const message = String(error.message || error || '').toLowerCase();

  /* 网络级别错误 → 重试 */
  if (message.includes('fetch failed') ||
      message.includes('network') ||
      message.includes('econnrefused') ||
      message.includes('econnreset') ||
      message.includes('etimedout') ||
      message.includes('timeout') ||
      message.includes('socket hang up') ||
      message.includes('dns') ||
      message.includes('abort')) {
    return true;
  }

  /* HTTP 429 限流 → 重试 */
  if (message.includes('429') || message.includes('too many requests') || message.includes('rate limit')) {
    return true;
  }

  /* HTTP 4xx/5xx 错误（含状态码的错误消息）→ 重试 */
  const statusMatch = message.match(/:\s*(\d{3})/);
  if (statusMatch) {
    const status = parseInt(statusMatch[1], 10);
    return status >= 400;
  }

  return false;
}

/**
 * 休眠指定毫秒
 */
function sleep(ms: number): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, ms));
}

/**
 * 带重试的异步操作执行器
 * - 自动重试可恢复的错误（网络错误、超时、HTTP 4xx/5xx）
 * - 指数退避避免过度请求
 * - 不可恢复的错误（SDK 内部参数校验错误等）直接抛出不重试
 *
 * @param fn 要执行的异步操作
 * @param options 重试配置
 */
export type RetryWithAttemptsResult<T> =
  | { ok: true; value: T; attempts: number }
  | { ok: false; error: unknown; attempts: number };

/**
 * 与 withRetry 相同，额外返回尝试次数（成功或最终失败时均有效）
 */
export async function withRetryWithAttempts<T>(
  fn: () => Promise<T>,
  options?: RetryOptions,
): Promise<RetryWithAttemptsResult<T>> {
  const opts = { ...DEFAULT_RETRY_OPTIONS, ...options };
  let lastError: any;

  for (let attempt = 0; attempt <= opts.maxRetries; attempt++) {
    const attempts = attempt + 1;
    try {
      const result = await fn();
      if (attempt > 0) {
        logger.info(`第 ${attempt + 1} 次尝试成功`);
      }
      return { ok: true, value: result, attempts };
    } catch (error: any) {
      lastError = error;
      const errorMsg = error.message || String(error);

      /* 最后一次尝试失败或不可重试的错误 → 直接抛出 */
      if (attempt >= opts.maxRetries || !opts.shouldRetry(error)) {
        if (attempt >= opts.maxRetries && opts.maxRetries > 0) {
          logger.error(`重试 ${opts.maxRetries} 次后仍失败: ${errorMsg}`);
        } else if (!opts.shouldRetry(error)) {
          logger.debug(`不可重试的错误: ${errorMsg}`);
        }
        return { ok: false, error, attempts };
      }

      /* 指数退避等待 */
      const delay = Math.min(opts.initialDelay * Math.pow(2, attempt), opts.maxDelay);
      logger.warn(`请求失败 (${errorMsg})，${delay}ms 后第 ${attempt + 2} 次重试...`);
      await sleep(delay);
    }
  }

  return { ok: false, error: lastError, attempts: opts.maxRetries + 1 };
}

export async function withRetry<T>(fn: () => Promise<T>, options?: RetryOptions): Promise<T> {
  const r = await withRetryWithAttempts(fn, options);
  if (r.ok) return r.value;
  throw r.error;
}

/**
 * 带超时控制的 fetch 包装
 * 在原生 fetch 基础上添加超时中断能力
 *
 * @param url 请求 URL
 * @param init fetch 选项
 * @param timeoutMs 超时时间（毫秒）
 */
/**
 * 缓存的全局配置快照，避免每次请求都读取
 * 仅在 setConfig 被调用时失效（通过 configVersion 比对）
 */
let _cachedFetchConfig: { fetchFn: typeof fetch; timeout: number; version: number } | null = null;

/**
 * 获取缓存的 fetch 配置
 */
function getFetchConfig(): { fetchFn: typeof fetch; timeout: number } {
  const config = getConfig();
  /* 简单的引用比对即可，getConfig 在未变更时返回同一对象 */
  if (!_cachedFetchConfig || _cachedFetchConfig.fetchFn !== (config.customFetch || fetch) || _cachedFetchConfig.timeout !== (config.timeout ?? DEFAULT_RETRY_OPTIONS.timeout)) {
    _cachedFetchConfig = {
      fetchFn: config.customFetch || fetch,
      timeout: config.timeout ?? DEFAULT_RETRY_OPTIONS.timeout,
      version: 0,
    };
  }
  return _cachedFetchConfig;
}

/** Node 下跳过 TLS 证书校验（用于 10mail.wangtz.cn 等渠道默认行为） */
export interface FetchWithTimeoutOptions {
  skipTlsVerify?: boolean;
}

function resolveFetchForTls(
  baseFetch: typeof fetch,
  skipTlsVerify: boolean | undefined,
): typeof fetch {
  if (!skipTlsVerify) {
    return baseFetch;
  }
  if (typeof process === 'undefined' || !process.versions?.node) {
    return baseFetch;
  }
  try {
    // Node 18+ 内置 undici，用于 per-request 关闭证书校验（不修改全局 NODE_TLS_REJECT_UNAUTHORIZED）
    // eslint-disable-next-line @typescript-eslint/no-require-imports
    const undici = require('undici') as {
      Agent: new (opts: { connect: { rejectUnauthorized: boolean } }) => unknown;
      fetch: (input: string, init?: Record<string, unknown>) => Promise<Response>;
    };
    const dispatcher = new undici.Agent({ connect: { rejectUnauthorized: false } });
    return ((u: string, i?: RequestInit) =>
      undici.fetch(u, {
        ...(i as Record<string, unknown>),
        dispatcher,
      })) as typeof fetch;
  } catch {
    return baseFetch;
  }
}

export async function fetchWithTimeout(
  url: string,
  init?: RequestInit,
  timeoutMs?: number,
  opts?: FetchWithTimeoutOptions,
): Promise<Response> {
  const { fetchFn, timeout: defaultTimeout } = getFetchConfig();
  const effectiveTimeout = timeoutMs ?? defaultTimeout;
  const fetchImpl = resolveFetchForTls(fetchFn, opts?.skipTlsVerify);
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), effectiveTimeout);

  /*
   * 如果调用方已提供 signal，需要同时监听两个信号（调用方 + 超时）
   * 任一触发则中断请求
   */
  const externalSignal = init?.signal;
  if (externalSignal) {
    externalSignal.addEventListener('abort', () => controller.abort(), { once: true });
  }

  try {
    const response = await fetchImpl(url, {
      ...init,
      signal: controller.signal,
    });
    return response;
  } catch (error: any) {
    if (error.name === 'AbortError') {
      throw new Error(`Request timeout after ${effectiveTimeout}ms: ${url}`);
    }
    throw error;
  } finally {
    clearTimeout(timeoutId);
  }
}
