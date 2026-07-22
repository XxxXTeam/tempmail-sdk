/*
 * SDK 主入口。
 * 渠道的新增/下线统一在 registry_channels.ts 中以单处 registerChannel({...}) 完成，
 * 本文件的渠道列表、信息映射、创建/收信分发逻辑全部由注册表自动派生。
 */

/* 引入注册文件（副作用导入）：模块加载时执行全部 registerChannel，填充注册表 */
import "./registry_channels";
import {
  ChannelInfo,
  channelRegistry,
  channelRegistryMap,
} from "./registry";
import {
  Channel,
  EmailInfo,
  InternalEmailInfo,
  Email,
  GetEmailsResult,
  GenerateEmailOptions,
  GetEmailsOptions,
} from "./types";
import { withRetry, withRetryWithAttempts } from "./retry";
import { reportTelemetry } from "./telemetry";
import { logger } from "./logger";
import { setConfig, getConfig } from "./config";
import { channelToBackend } from "./backend-groups";
import {
  isBackendOpen,
  recordBackendFailure,
  recordBackendSuccess,
} from "./circuit-breaker";
import { filterChannelsByDomains } from "./channel-domains";

export {
  Channel,
  EmailInfo,
  Email,
  EmailAttachment,
  GetEmailsResult,
  GenerateEmailOptions,
  GetEmailsOptions,
} from "./types";

/**
 * SDK 内部 token 存储
 * 使用 WeakMap 将 EmailInfo 对象映射到对应的 token
 * 用户无法直接访问 token，由 SDK 自动管理
 * @internal
 */
const tokenStore = new WeakMap<EmailInfo, string>();
export { normalizeEmail } from "./normalize";
export { CHANNEL_DOMAINS, DYNAMIC_DOMAIN_CHANNELS } from "./channel-domains";
export {
  withRetry,
  withRetryWithAttempts,
  fetchWithTimeout,
  RetryOptions,
} from "./retry";
export type { RetryWithAttemptsResult, FetchWithTimeoutOptions } from "./retry";
export {
  LogLevel,
  LogHandler,
  setLogLevel,
  getLogLevel,
  setLogger,
  getLogger,
  logger,
} from "./logger";
export { SDKConfig, setConfig, getConfig } from "./config";
export { getSdkVersion } from "./version";

export { ChannelInfo } from "./registry";

/* WebUI 嵌入式服务器：导出控制函数，并在模块加载时按环境变量自动启动 */
export { startWebUI, stopWebUI } from "./webui";

/** 所有支持的公共渠道列表（由注册表按注册顺序派生）；随机尝试顺序会基于该列表在本端独立洗牌，不要求跨 SDK 一致 */
const allChannels: Channel[] = channelRegistry.map((spec) => spec.channel);

/**
 * 获取所有支持的渠道列表
 *
 * @returns 所有渠道的信息数组
 *
 * @example
 * ```ts
 * const channels = listChannels();
 * channels.forEach(ch => console.log(`${ch.name} (${ch.website})`));
 * ```
 */
export function listChannels(): ChannelInfo[] {
  return channelRegistry.map((spec) => ({
    channel: spec.channel,
    name: spec.name,
    website: spec.website,
  }));
}

/**
 * 获取指定渠道的详细信息
 *
 * @param channel - 渠道标识
 * @returns 渠道信息，不存在时返回 undefined
 */
export function getChannelInfo(channel: Channel): ChannelInfo | undefined {
  const spec = channelRegistryMap.get(channel);
  if (!spec) return undefined;
  return { channel: spec.channel, name: spec.name, website: spec.website };
}

/**
 * 创建临时邮箱
 *
 * 错误处理策略：
 * - 指定渠道失败时，默认自动尝试其他可用渠道（打乱顺序逐个尝试）
 * - `channelFallback: false` 且指定了 `channel` 时，仅尝试该渠道，失败即返回 null
 * - 未指定渠道时，打乱全部渠道逐个尝试，直到成功
 * - 所有渠道均不可用时返回 null（不抛出异常）
 *
 * @param options - 创建选项，可指定渠道、有效时长、域名等
 * @returns 邮箱信息，所有渠道均不可用时返回 null
 */
export async function generateEmail(
  options: GenerateEmailOptions = {},
): Promise<EmailInfo | null> {
  /*
   * 构建尝试顺序：
   * - 指定渠道 → 优先尝试该渠道，失败后随机尝试其他渠道
   * - 未指定 → 打乱全部渠道逐个尝试
   */
  const allowFallback = options.channelFallback !== false;
  const targetDomains = extractTargetDomains(options);
  const tryOrder = buildChannelOrder(options.channel, allowFallback, targetDomains);
  const maxChannels = options.maxChannelsTried ?? 20;
  const totalTimeout = options.totalTimeout ?? 60_000;
  const startTime = Date.now();
  const failedBackends = new Set<string>();

  let channelsTried = 0;
  let lastErr = "";
  for (const ch of tryOrder) {
    if (channelsTried >= maxChannels) {
      logger.warn(`已尝试 ${maxChannels} 个渠道，停止尝试`);
      break;
    }
    if (Date.now() - startTime >= totalTimeout) {
      logger.warn(`整体超时 ${totalTimeout}ms，停止尝试`);
      break;
    }

    const backend = channelToBackend.get(ch);

    if (backend) {
      if (failedBackends.has(backend)) {
        logger.debug(`跳过渠道 ${ch}，同后端 ${backend} 已失败`);
        continue;
      }
      if (!isBackendOpen(backend)) {
        logger.debug(`跳过渠道 ${ch}，后端 ${backend} 处于熔断状态`);
        continue;
      }
    }

    channelsTried += 1;
    logger.info(`创建临时邮箱, 渠道: ${ch}`);
    const r = await withRetryWithAttempts(
      () => generateEmailOnce(ch, options),
      options.retry,
    );
    if (r.ok) {
      const internal = r.value;
      logger.info(`邮箱创建成功: ${internal.email} (渠道: ${ch})`);
      reportTelemetry(
        "generate_email",
        ch,
        true,
        r.attempts,
        channelsTried,
        "",
      );
      if (backend) {
        recordBackendSuccess(backend);
      }

      /* 将 token 存入内部存储，不暴露给用户 */
      const publicInfo: EmailInfo = {
        channel: internal.channel,
        email: internal.email,
        expiresAt: internal.expiresAt,
        createdAt: internal.createdAt,
      };
      if (internal.token) {
        tokenStore.set(publicInfo, internal.token);
      }
      return publicInfo;
    }
    const msg = (r.error as any)?.message || String(r.error);
    lastErr = msg;
    logger.warn(`渠道 ${ch} 不可用: ${msg}，尝试下一个渠道`);

    if (backend) {
      failedBackends.add(backend);
      recordBackendFailure(backend);
    }
  }

  logger.error("所有渠道均不可用，创建邮箱失败");
  reportTelemetry("generate_email", "", false, 0, channelsTried, lastErr);
  return null;
}

/**
 * Fisher-Yates 洗牌算法，生成均匀分布的随机排列
 */
function shuffleArray<T>(arr: T[]): T[] {
  const result = [...arr];
  for (let i = result.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    [result[i], result[j]] = [result[j], result[i]];
  }
  return result;
}

/**
 * 从 suffix 和 domains 选项中提取目标域名列表
 * suffix 如 "@gmail.com" 会提取出 "gmail.com"
 */
function extractTargetDomains(options: GenerateEmailOptions): string[] {
  const targets: string[] = [];

  if (options.suffix) {
    /* 去掉前导 @ 符号 */
    const domain = options.suffix.startsWith("@")
      ? options.suffix.slice(1)
      : options.suffix;
    if (domain) {
      targets.push(domain);
    }
  }

  if (options.domains && options.domains.length > 0) {
    for (const d of options.domains) {
      const domain = d.startsWith("@") ? d.slice(1) : d;
      if (domain && !targets.includes(domain)) {
        targets.push(domain);
      }
    }
  }

  return targets;
}

/**
 * 构建渠道尝试顺序
 * 指定渠道时优先尝试该渠道，其余渠道打乱追加
 * 未指定时打乱全部渠道
 * 支持按目标域名筛选渠道
 * 本函数返回的是本端运行时随机顺序，不作为跨 SDK 一致性约束
 */
function buildChannelOrder(
  preferred?: Channel,
  allowFallback = true,
  targetDomains: string[] = [],
): Channel[] {
  let candidates = allChannels as Channel[];

  /* 按域名筛选渠道 */
  if (targetDomains.length > 0) {
    candidates = filterChannelsByDomains(candidates, targetDomains);
  }

  /* 指定渠道且关闭 fallback 时无需洗牌，提前返回省去整数组拷贝 */
  if (preferred && !allowFallback) {
    return [preferred];
  }

  const shuffled = shuffleArray(candidates);
  if (!preferred) {
    return shuffled;
  }
  const rest = shuffled.filter((ch) => ch !== preferred);
  return [preferred, ...rest];
}

/**
 * 单次创建邮箱（不含重试逻辑）
 * 根据渠道标识从注册表分发到对应 provider 实现
 */
async function generateEmailOnce(
  channel: Channel,
  options: GenerateEmailOptions,
): Promise<InternalEmailInfo> {
  const spec = channelRegistryMap.get(channel);
  if (!spec) {
    throw new Error(`Unknown channel: ${channel}`);
  }
  return spec.generate(options);
}

/**
 * 获取邮件列表
 * Channel/Email/Token 等由 SDK 从 EmailInfo 中自动获取，用户无需手动传递
 *
 * 错误处理策略：
 * - 网络错误、超时、429、服务端 5xx 错误 → 自动重试（默认 2 次）
 * - 重试耗尽后返回 { success: false, emails: [] }，不抛异常
 * - 参数校验错误（缺少 EmailInfo）直接抛出
 *
 * @param info - GenerateEmail() 返回的邮箱信息
 * @param options - 可选配置（重试等）
 * @returns 邮件结果，包含 success 标记和邮件列表
 */
export async function getEmails(
  info: EmailInfo,
  options?: GetEmailsOptions,
): Promise<GetEmailsResult> {
  if (!info) {
    reportTelemetry(
      "get_emails",
      "",
      false,
      0,
      0,
      "EmailInfo is required, call generateEmail() first",
    );
    throw new Error("EmailInfo is required, call generateEmail() first");
  }

  const { channel, email } = info;
  const token = tokenStore.get(info);

  if (!channel) {
    reportTelemetry("get_emails", "", false, 0, 0, "Channel is required");
    throw new Error("Channel is required");
  }
  if (!email && channel !== "tempmail-lol") {
    reportTelemetry("get_emails", channel, false, 0, 0, "Email is required");
    throw new Error("Email is required");
  }

  logger.debug(`获取邮件, 渠道: ${channel}, 邮箱: ${email}`);
  const gr = await withRetryWithAttempts(
    () => getEmailsOnce(channel, email, token),
    options?.retry,
  );
  if (gr.ok) {
    const emails = gr.value;
    if (emails.length > 0) {
      logger.info(`获取到 ${emails.length} 封邮件, 渠道: ${channel}`);
    } else {
      logger.debug(`暂无邮件, 渠道: ${channel}`);
    }
    reportTelemetry("get_emails", channel, true, gr.attempts, 0, "");
    return { channel, email, emails, success: true };
  }
  const err = gr.error as any;
  const errMsg = err?.message || String(err);
  /*
   * 重试耗尽后仍然失败 → 返回空结果而非抛异常
   * 这样调用方在轮询场景下不会因为一次网络波动而中断整个流程
   */
  logger.error(`获取邮件失败, 渠道: ${channel}, 错误: ${errMsg}`);
  reportTelemetry("get_emails", channel, false, gr.attempts, 0, errMsg);
  return { channel, email, emails: [], success: false };
}

/**
 * 单次获取邮件（不含重试逻辑）
 * 根据渠道标识从注册表分发到对应 provider 实现，token 校验由各渠道闭包内部完成
 */
async function getEmailsOnce(
  channel: Channel,
  email: string,
  token?: string,
): Promise<Email[]> {
  const spec = channelRegistryMap.get(channel);
  if (!spec) {
    throw new Error(`Unknown channel: ${channel}`);
  }
  return spec.getEmails(email, token);
}

/**
 * 临时邮箱客户端
 * 封装了邮箱创建和邮件获取的完整流程，自动管理邮箱信息和认证令牌
 *
 * @example
 * ```ts
 * const client = new TempEmailClient();
 * const emailInfo = await client.generate({ channel: 'mail-tm' });
 * console.log('邮箱:', emailInfo.email);
 *
 * // 轮询获取邮件
 * const result = await client.getEmails();
 * if (result.success) {
 *   console.log('邮件数:', result.emails.length);
 * }
 * ```
 */
export class TempEmailClient {
  private emailInfo: EmailInfo | null = null;

  /**
   * 创建临时邮箱并缓存邮箱信息
   * 后续调用 getEmails() 时自动使用此邮箱的渠道、地址和令牌
   * 所有渠道均不可用时返回 null
   */
  async generate(
    options: GenerateEmailOptions = {},
  ): Promise<EmailInfo | null> {
    this.emailInfo = await generateEmail(options);
    return this.emailInfo;
  }

  /**
   * 获取当前邮箱的邮件列表
   * 必须先调用 generate() 创建邮箱
   *
   * @throws 未调用 generate() 时抛出异常
   */
  async getEmails(options?: GetEmailsOptions): Promise<GetEmailsResult> {
    if (!this.emailInfo) {
      reportTelemetry(
        "get_emails",
        "",
        false,
        0,
        0,
        "No email generated. Call generate() first.",
      );
      throw new Error("No email generated. Call generate() first.");
    }

    return getEmails(this.emailInfo, options);
  }

  /**
   * 获取当前缓存的邮箱信息
   * 未调用 generate() 时返回 null
   */
  getEmailInfo(): EmailInfo | null {
    return this.emailInfo;
  }
}

export default {
  listChannels,
  getChannelInfo,
  generateEmail,
  getEmails,
  TempEmailClient,
  setConfig,
  getConfig,
};
