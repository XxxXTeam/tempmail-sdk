/**
 * SDK 日志模块
 * 提供分级日志能力，支持自定义日志处理器
 * 默认静默不输出，用户可通过 setLogLevel / setLogger 启用
 */

/**
 * 日志级别枚举
 * 数值越小级别越高，设置某级别后只输出该级别及以上的日志
 */
export enum LogLevel {
  /** 关闭所有日志 */
  SILENT = 0,
  /** 错误日志：请求失败、重试耗尽等 */
  ERROR = 1,
  /** 警告日志：重试中、降级处理等 */
  WARN = 2,
  /** 信息日志：请求开始、完成等关键流程 */
  INFO = 3,
  /** 调试日志：请求详情、响应内容等 */
  DEBUG = 4,
}

/**
 * 日志处理器接口
 * 用户可实现此接口来自定义日志输出方式（如写文件、发送到远程等）
 */
export interface LogHandler {
  error(message: string, ...args: any[]): void;
  warn(message: string, ...args: any[]): void;
  info(message: string, ...args: any[]): void;
  debug(message: string, ...args: any[]): void;
}

/**
 * 默认日志处理器，直接输出到 console
 */
const defaultHandler: LogHandler = {
  error: (msg, ...args) => console.error(msg, ...args),
  warn: (msg, ...args) => console.warn(msg, ...args),
  info: (msg, ...args) => console.info(msg, ...args),
  debug: (msg, ...args) => console.debug(msg, ...args),
};

let currentLevel: LogLevel = LogLevel.SILENT;
let currentHandler: LogHandler = defaultHandler;

/**
 * 设置日志级别
 * 默认 SILENT（不输出任何日志）
 *
 * @example
 * ```ts
 * import { setLogLevel, LogLevel } from 'tempmail-sdk';
 * setLogLevel(LogLevel.DEBUG); // 开启所有日志
 * setLogLevel(LogLevel.INFO);  // 只输出 INFO 及以上
 * ```
 */
export function setLogLevel(level: LogLevel): void {
  currentLevel = level;
}

/**
 * 获取当前日志级别
 */
export function getLogLevel(): LogLevel {
  return currentLevel;
}

/**
 * 设置自定义日志处理器
 * 替换默认的 console 输出
 *
 * @example
 * ```ts
 * import { setLogger } from 'tempmail-sdk';
 * setLogger({
 *   error: (msg, ...args) => myLogger.error(msg, ...args),
 *   warn:  (msg, ...args) => myLogger.warn(msg, ...args),
 *   info:  (msg, ...args) => myLogger.info(msg, ...args),
 *   debug: (msg, ...args) => myLogger.debug(msg, ...args),
 * });
 * ```
 */
export function setLogger(handler: LogHandler): void {
  currentHandler = handler;
}

/**
 * SDK 内部日志工具
 * 根据当前日志级别过滤输出
 */
export const logger = {
  error(msg: string, ...args: any[]): void {
    if (currentLevel >= LogLevel.ERROR) currentHandler.error(msg, ...args);
  },
  warn(msg: string, ...args: any[]): void {
    if (currentLevel >= LogLevel.WARN) currentHandler.warn(msg, ...args);
  },
  info(msg: string, ...args: any[]): void {
    if (currentLevel >= LogLevel.INFO) currentHandler.info(msg, ...args);
  },
  debug(msg: string, ...args: any[]): void {
    if (currentLevel >= LogLevel.DEBUG) currentHandler.debug(msg, ...args);
  },
};
