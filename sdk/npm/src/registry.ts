import {
  Channel,
  Email,
  InternalEmailInfo,
  GenerateEmailOptions,
} from "./types";

/**
 * 渠道信息，包含渠道标识、显示名称和对应网站
 */
export interface ChannelInfo {
  /** 渠道标识 */
  channel: Channel;
  /** 渠道显示名称 */
  name: string;
  /** 对应的临时邮箱服务网站 */
  website: string;
}

/**
 * 单个渠道的注册规格。
 * 每新增一个渠道只需在注册文件里追加一处 registerChannel({...})，
 * 渠道列表（allChannels）、信息映射表（channelInfoMap）、创建/收信分发逻辑
 * 全部由该结构自动派生，无需再手动同步多处平行结构。
 */
export interface ChannelSpec {
  /** 渠道标识 */
  channel: Channel;
  /** 渠道显示名称 */
  name: string;
  /** 对应的临时邮箱服务网站 */
  website: string;
  /** 创建邮箱的实现（对应原 generateEmailOnce 中该渠道的 case 体） */
  generate: (options: GenerateEmailOptions) => Promise<InternalEmailInfo>;
  /** 获取邮件的实现（对应原 getEmailsOnce 中该渠道的 case 体） */
  getEmails: (email: string, token?: string) => Promise<Email[]>;
}

/** 有序渠道注册表，注册顺序即枚举顺序（硬约束，五端一致） */
export const channelRegistry: ChannelSpec[] = [];

/** 渠道标识到注册规格的映射，便于 O(1) 查找分发 */
export const channelRegistryMap = new Map<Channel, ChannelSpec>();

/**
 * 注册一个渠道。
 * 追加到有序数组并建立映射；重复注册同一 Channel 视为编程错误，直接抛错。
 * @param spec 渠道注册规格
 */
export function registerChannel(spec: ChannelSpec): void {
  if (channelRegistryMap.has(spec.channel)) {
    throw new Error(`duplicate channel registration: ${spec.channel}`);
  }
  channelRegistry.push(spec);
  channelRegistryMap.set(spec.channel, spec);
}
