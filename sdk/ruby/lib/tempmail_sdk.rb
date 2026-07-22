# frozen_string_literal: true

# 临时邮箱 SDK (Ruby)
# 聚合 279 个第三方临时邮箱渠道，所有渠道返回统一标准化格式。
# 与 Go / npm / Rust / Python / C / PHP 各端共享相同的渠道标识与顺序。

require_relative "tempmail_sdk/version"
require_relative "tempmail_sdk/types"
require_relative "tempmail_sdk/config"
require_relative "tempmail_sdk/logger"
require_relative "tempmail_sdk/html_utils"
require_relative "tempmail_sdk/normalize"
require_relative "tempmail_sdk/http"
require_relative "tempmail_sdk/retry"
require_relative "tempmail_sdk/telemetry"
require_relative "tempmail_sdk/channels"
require_relative "tempmail_sdk/registry"
require_relative "tempmail_sdk/client"
require_relative "tempmail_sdk/webui"

# 顶层命名空间：聚合各端一致的临时邮箱 SDK 能力
module TempmailSdk
  module_function

  # 创建临时邮箱（无状态入口，支持渠道 fallback）
  # @param options [GenerateEmailOptions, nil]
  # @return [EmailInfo, nil]
  def generate_email(options = nil)
    Client.generate_email(options)
  end

  # 获取邮件列表（无状态入口）
  # @param info [EmailInfo]
  # @param options [GetEmailsOptions, nil]
  # @return [GetEmailsResult]
  def get_emails(info, options = nil)
    Client.get_emails(info, options)
  end

  # 获取所有支持的渠道列表（有序）
  # @return [Array<ChannelInfo>]
  def list_channels
    Client.list_channels
  end

  # 获取指定渠道的详细信息
  # @param channel [String]
  # @return [ChannelInfo, nil]
  def channel_info(channel)
    Client.channel_info(channel)
  end

  # 设置 SDK 全局配置
  def set_config(config = nil, **kwargs)
    Config.set_config(config, **kwargs)
  end

  # 获取当前全局配置
  # @return [SDKConfig]
  def config
    Config.get_config
  end

  # 设置日志级别
  def set_log_level(level)
    Log.set_log_level(level)
  end

  # @return [String] SDK 版本号
  def sdk_version
    VERSION
  end
end

# 加载后按环境变量自动启动 WebUI（默认不启动）
TempmailSdk::WebUI.auto_start
