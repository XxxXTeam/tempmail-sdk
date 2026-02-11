/**
 * 通用重试工具
 * 提供请求重试、超时控制、指数退避等错误恢复机制
 */

import { logger } from './logger';

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
 * 网络错误、超时、HTTP 4xx/5xx 错误均可重试
 * 仅参数校验类错误（由 SDK 内部抛出）不重试
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

  /* HTTP 4xx/5xx 错误 → 重试 */
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
 * - 自动重试可恢复的错误（网络错误、超时、5xx）
 * - 指数退避避免过度请求
 * - 不可恢复的错误（4xx 参数错误等）直接抛出不重试
 *
 * @param fn 要执行的异步操作
 * @param options 重试配置
 */
export async function withRetry<T>(fn: () => Promise<T>, options?: RetryOptions): Promise<T> {
  const opts = { ...DEFAULT_RETRY_OPTIONS, ...options };
  let lastError: any;

  for (let attempt = 0; attempt <= opts.maxRetries; attempt++) {
    try {
      const result = await fn();
      if (attempt > 0) {
        logger.info(`第 ${attempt + 1} 次尝试成功`);
      }
      return result;
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
        throw error;
      }

      /* 指数退避等待 */
      const delay = Math.min(opts.initialDelay * Math.pow(2, attempt), opts.maxDelay);
      logger.warn(`请求失败 (${errorMsg})，${delay}ms 后第 ${attempt + 2} 次重试...`);
      await sleep(delay);
    }
  }

  throw lastError;
}

/**
 * 带超时控制的 fetch 包装
 * 在原生 fetch 基础上添加超时中断能力
 *
 * @param url 请求 URL
 * @param init fetch 选项
 * @param timeoutMs 超时时间（毫秒）
 */
export async function fetchWithTimeout(
  url: string,
  init?: RequestInit,
  timeoutMs: number = DEFAULT_RETRY_OPTIONS.timeout,
): Promise<Response> {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), timeoutMs);

  try {
    const response = await fetch(url, {
      ...init,
      signal: controller.signal,
    });
    return response;
  } catch (error: any) {
    if (error.name === 'AbortError') {
      throw new Error(`Request timeout after ${timeoutMs}ms: ${url}`);
    }
    throw error;
  } finally {
    clearTimeout(timeoutId);
  }
}
