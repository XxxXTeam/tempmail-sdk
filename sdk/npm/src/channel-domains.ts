/**
 * 渠道域名映射表
 * 维护各渠道已知支持的邮箱域名，用于后缀筛选功能
 */

import type { Channel } from "./types";

/**
 * 已知固定域名的渠道映射
 * key: 渠道标识，value: 该渠道支持的邮箱域名列表
 */
export const CHANNEL_DOMAINS: Partial<Record<Channel, string[]>> = {
  // Gmail 相关
  "emailnator": ["gmail.com", "googlemail.com"],
  "tempgmailer": ["gmail.com"],

  // 多域名渠道
  "catchmail": ["catchmail.io", "mailistry.com", "zeppost.com"],
  "mailforspam": ["mailforspam.com", "tempmail.io", "disposable.email"],
  "tempmail365": ["fengyou.cc", "shop345.com", "nutemail.com", "qvrf.cn"],
  "tempinbox": ["tempinbox.xyz", "thepiratebay.cloud", "cryptoblad.nl"],
  "10minute-one": ["xghff.com", "oqqaj.com", "psovv.com", "dbwot.com", "ygwpr.com", "imxwe.com"],
  "apihz": ["apimail.email", "apimail.vip"],
  "24mail-chacuo": ["chacuo.net", "027168.com"],

  // 单域名渠道 - mailinator 系列
  "mailinator": ["mailinator.com"],
  "sogetthis-com": ["sogetthis.com"],
  "bobmail-info": ["bobmail.info"],
  "suremail-info": ["suremail.info"],
  "binkmail-com": ["binkmail.com"],
  "veryrealemail-com": ["veryrealemail.com"],
  "chammy-info": ["chammy.info"],
  "thisisnotmyrealemail-com": ["thisisnotmyrealemail.com"],
  "notmailinator-com": ["notmailinator.com"],
  "spamhereplease-com": ["spamhereplease.com"],
  "sendspamhere-com": ["sendspamhere.com"],
  "sendfree-org": ["sendfree.org"],

  // 其他单域名
  "mailnesia": ["mailnesia.com"],
  "harakirimail": ["harakirimail.com"],
  "byom": ["byom.de"],
  "eyepaste": ["eyepaste.com"],
  "mailcatch": ["mailcatch.com"],
  "maildrop-cc": ["maildrop.cc"],
  "smail-pw": ["smail.pw"],
  "inboxkitten": ["inboxkitten.com"],
  "mffac": ["mffac.com"],
  "nimail": ["nimail.cn"],
  "mailgolem": ["mailgolem.com"],
  "temp-mail-org": ["diarshop.com", "gicont.com", "suahi.com"],
  "minuteinbox": ["minafter.com"],
  "mohmal": ["emailinbo.live"],
  "10minutemail-net": ["laoia.com", "acgia.com"],
  "xkx-me": ["xkx.me"],
};

/**
 * tempmail-plus 系列渠道域名映射
 */
export const CHANNEL_DOMAINS_PLUS: Partial<Record<Channel, string[]>> = {
  "tempmail-plus": ["mailto.plus"],
  "fexpost-com": ["fexpost.com"],
  "fexbox-org": ["fexbox.org"],
  "mailbox-in-ua": ["mailbox.in.ua"],
  "rover-info": ["rover.info"],
  "chitthi-in": ["chitthi.in"],
  "fextemp-com": ["fextemp.com"],
  "any-pink": ["any.pink"],
  "merepost-com": ["merepost.com"],
};

/* 合并所有已知域名映射 */
Object.assign(CHANNEL_DOMAINS, CHANNEL_DOMAINS_PLUS);

/**
 * 域名通过 API 动态获取的渠道列表
 * 这些渠道的可用域名需要在运行时查询，无法预先确定
 * 在后缀筛选时默认保留这些渠道（让它们在运行时自行尝试）
 */
export const DYNAMIC_DOMAIN_CHANNELS: Channel[] = [
  "mail-tm", "duckmail", "mail-td", "chatgpt-org-uk", "temporam",
  "getnada", "neighbours", "neighbours-sh", "inboxes", "mailmomy",
  "freecustom", "mail-sunls", "anonymmail", "fake-legal", "maildrop",
  "mail-cx", "moakt", "tempmail-lol", "temp-mail-org",
];

/**
 * 判断域名是否匹配目标域名（支持子域名匹配）
 * 例如：matchDomain("mail.gmail.com", "gmail.com") => true
 *
 * @param domain - 渠道支持的域名
 * @param target - 用户指定的目标域名
 * @returns 是否匹配
 */
export function matchDomain(domain: string, target: string): boolean {
  const d = domain.toLowerCase();
  const t = target.toLowerCase();
  return d === t || d.endsWith("." + t);
}

/* 动态域名渠道集合，模块级预建复用，避免每次筛选重复构造 */
const DYNAMIC_DOMAIN_SET = new Set<string>(DYNAMIC_DOMAIN_CHANNELS);

/**
 * 根据目标域名列表筛选渠道
 * 返回支持目标域名的渠道列表
 *
 * @param channels - 待筛选的渠道列表
 * @param targetDomains - 用户指定的目标域名列表
 * @returns 筛选后的渠道列表，如果无匹配则返回原列表（fallback）
 */
export function filterChannelsByDomains(
  channels: Channel[],
  targetDomains: string[],
): Channel[] {
  if (!targetDomains.length) {
    return channels;
  }

  const filtered = channels.filter((ch) => {
    /* 动态域名渠道默认保留 */
    if (DYNAMIC_DOMAIN_SET.has(ch)) {
      return true;
    }

    /* 检查渠道已知域名是否与目标域名匹配 */
    const domains = CHANNEL_DOMAINS[ch];
    if (!domains) {
      return false;
    }

    return domains.some((d) =>
      targetDomains.some((t) => matchDomain(d, t) || matchDomain(t, d)),
    );
  });

  /* 如果过滤后没有任何渠道匹配，fallback 到全部渠道 */
  if (filtered.length === 0) {
    return channels;
  }

  return filtered;
}
