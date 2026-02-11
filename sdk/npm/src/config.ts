/**
 * SDK 全局配置
 * 包含代理、超时、SSL 等设置，作用于所有 HTTP 请求
 *
 * 支持环境变量自动读取（优先级低于代码设置）：
 *   TEMPMAIL_PROXY    - 代理 URL
 *   TEMPMAIL_TIMEOUT  - 超时毫秒数
 *   TEMPMAIL_INSECURE - 设为 "1" 或 "true" 跳过 SSL 验证
 *
 * Node.js 环境下设置 insecure 会自动设置 NODE_TLS_REJECT_UNAUTHORIZED=0
 */

/**
 * SDK 全局配置接口
 */
export interface SDKConfig {
  /** 代理 URL，支持 http/https/socks5，如 "http://127.0.0.1:7890" */
  proxy?: string;
  /** 全局默认超时（毫秒），默认 15000 */
  timeout?: number;
  /** 跳过 SSL 证书验证（调试用） */
  insecure?: boolean;
  /**
   * 自定义 fetch 函数，用于完全控制 HTTP 请求行为
   * 当设置了 proxy 但环境不支持 undici 时，可通过此选项传入支持代理的 fetch 实现
   */
  customFetch?: typeof fetch;
}

/** 从环境变量读取默认配置 */
function loadEnvConfig(): SDKConfig {
  const config: SDKConfig = {};
  if (typeof process !== 'undefined' && process.env) {
    if (process.env.TEMPMAIL_PROXY) {
      config.proxy = process.env.TEMPMAIL_PROXY;
    }
    if (process.env.TEMPMAIL_TIMEOUT) {
      const t = parseInt(process.env.TEMPMAIL_TIMEOUT, 10);
      if (!isNaN(t) && t > 0) config.timeout = t;
    }
    if (process.env.TEMPMAIL_INSECURE === '1' || process.env.TEMPMAIL_INSECURE?.toLowerCase() === 'true') {
      config.insecure = true;
    }
  }
  return config;
}

let globalConfig: SDKConfig = loadEnvConfig();

/**
 * 设置 SDK 全局配置
 *
 * @example
 * ```ts
 * // 一行跳过 SSL 验证
 * setConfig({ insecure: true });
 *
 * // 设置代理和超时
 * setConfig({ proxy: 'http://127.0.0.1:7890', timeout: 30000 });
 *
 * // 使用自定义 fetch（如支持代理的 undici）
 * import { ProxyAgent, fetch as undiciFetch } from 'undici';
 * const agent = new ProxyAgent('http://127.0.0.1:7890');
 * setConfig({ customFetch: (url, init) => undiciFetch(url, { ...init, dispatcher: agent }) });
 * ```
 */
export function setConfig(config: SDKConfig): void {
  globalConfig = { ...config };
  /* Node.js 环境下 insecure 自动设置环境变量 */
  if (typeof process !== 'undefined' && process.env) {
    if (config.insecure) {
      process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';
    } else {
      delete process.env.NODE_TLS_REJECT_UNAUTHORIZED;
    }
  }
}

/** 获取当前 SDK 全局配置 */
export function getConfig(): SDKConfig {
  return globalConfig;
}
