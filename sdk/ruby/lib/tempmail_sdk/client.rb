# frozen_string_literal: true

module TempmailSdk
  # SDK 主入口：聚合所有渠道逻辑，提供统一的无状态 API 与有状态 Client
  module Client
    module_function

    # 所有支持的公共渠道标识（有序），由注册表派生
    def all_channels
      Registry::REGISTRY.map(&:channel)
    end

    # 渠道信息映射表，由注册表派生
    def channel_info_map
      @channel_info_map ||= Registry::REGISTRY.each_with_object({}) do |spec, h|
        h[spec.channel] = ChannelInfo.new(channel: spec.channel, name: spec.name, website: spec.website)
      end
    end

    # 获取所有支持的渠道列表（有序，与 baseline 逐行一致）
    # @return [Array<ChannelInfo>]
    def list_channels
      map = channel_info_map
      all_channels.map { |ch| map[ch] }
    end

    # 获取指定渠道的详细信息
    # @param channel [String]
    # @return [ChannelInfo, nil]
    def channel_info(channel)
      channel_info_map[channel]
    end

    # 创建临时邮箱
    #
    # 错误处理策略：
    # - 指定渠道失败时，自动尝试其他可用渠道（打乱顺序逐个尝试）
    # - 未指定渠道时，打乱全部渠道逐个尝试，直到成功
    # - 所有渠道均不可用时返回 nil（不抛出异常）
    #
    # @param options [GenerateEmailOptions, nil]
    # @return [EmailInfo, nil]
    def generate_email(options = nil)
      options ||= GenerateEmailOptions.new
      logger = Log.logger
      try_order = build_channel_order(options.channel)

      max_channels = options.max_channels_tried.to_i.positive? ? options.max_channels_tried : 20
      total_timeout = options.total_timeout.to_f.positive? ? options.total_timeout : 60.0
      start_time = Time.now
      channels_tried = 0
      last_err = ""

      try_order.each do |ch|
        if channels_tried >= max_channels
          logger.warn("已尝试最大渠道数 (#{max_channels})，停止")
          break
        end
        if Time.now - start_time >= total_timeout
          logger.warn("整体超时，停止尝试")
          break
        end

        channels_tried += 1
        logger.info("创建临时邮箱, 渠道: #{ch}")
        r = Retry.with_retry_with_attempts(options.retry_config) { generate_email_once(ch, options) }
        if r.ok?
          result = r.value
          logger.info("邮箱创建成功: #{result.email} (渠道: #{ch})")
          Telemetry.report(operation: "generate_email", channel: ch, success: true,
                           attempt_count: r.attempts, channels_tried: channels_tried, error: "")
          return result
        end

        last_err = r.error ? r.error.message.to_s : "unknown"
        logger.warn("渠道 #{ch} 不可用: #{last_err}，尝试下一个渠道")
      end

      logger.error("所有渠道均不可用，创建邮箱失败")
      Telemetry.report(operation: "generate_email", channel: "", success: false,
                       attempt_count: 0, channels_tried: channels_tried, error: last_err)
      nil
    end

    # 构建渠道尝试顺序：指定渠道优先，其余打乱追加；未指定则全部打乱
    # 返回值为本端运行时随机顺序，不作为跨 SDK 一致性约束
    def build_channel_order(preferred = nil)
      shuffled = all_channels.shuffle
      return shuffled unless preferred

      [preferred] + shuffled.reject { |ch| ch == preferred }
    end

    # 单次创建邮箱（不含重试逻辑）
    def generate_email_once(channel, options)
      spec = Registry::REGISTRY_MAP[channel]
      raise "Unknown channel: #{channel}" if spec.nil?

      spec.generate.call(options)
    end

    # 获取邮件列表
    # Channel/Email/Token 等由 SDK 从 EmailInfo 中自动获取，用户无需手动传递
    #
    # @param info [EmailInfo]
    # @param options [GetEmailsOptions, nil]
    # @return [GetEmailsResult]
    def get_emails(info, options = nil)
      if info.nil?
        Telemetry.report(operation: "get_emails", channel: "", success: false, attempt_count: 0,
                         channels_tried: 0, error: "EmailInfo is required, call generate_email() first")
        raise ArgumentError, "EmailInfo is required, call generate_email() first"
      end

      channel = info.channel
      email = info.email
      token = info.token
      retry_config = options&.retry_config
      logger = Log.logger

      if channel.nil? || channel.empty?
        Telemetry.report(operation: "get_emails", channel: "", success: false, attempt_count: 0,
                         channels_tried: 0, error: "channel is required")
        raise ArgumentError, "channel is required"
      end
      if (email.nil? || email.empty?) && channel != "tempmail-lol"
        Telemetry.report(operation: "get_emails", channel: channel, success: false, attempt_count: 0,
                         channels_tried: 0, error: "email is required")
        raise ArgumentError, "email is required"
      end

      logger.debug("获取邮件, 渠道: #{channel}, 邮箱: #{email}")
      r = Retry.with_retry_with_attempts(retry_config) { get_emails_once(channel, email, token) }

      if r.ok?
        emails = r.value
        if emails.empty?
          logger.debug("暂无邮件, 渠道: #{channel}")
        else
          logger.info("获取到 #{emails.size} 封邮件, 渠道: #{channel}")
        end
        Telemetry.report(operation: "get_emails", channel: channel, success: true,
                         attempt_count: r.attempts, channels_tried: 0, error: "")
        return GetEmailsResult.new(channel: channel, email: email, emails: emails, success: true)
      end

      err_s = r.error ? r.error.message.to_s : "unknown"
      logger.error("获取邮件失败, 渠道: #{channel}, 错误: #{err_s}")
      Telemetry.report(operation: "get_emails", channel: channel, success: false,
                       attempt_count: r.attempts, channels_tried: 0, error: err_s)
      GetEmailsResult.new(channel: channel, email: email, emails: [], success: false)
    end

    # 单次获取邮件（不含重试逻辑）
    def get_emails_once(channel, email, token)
      spec = Registry::REGISTRY_MAP[channel]
      raise "Unknown channel: #{channel}" if spec.nil?

      spec.get_emails.call(email, token)
    end
  end

  # 临时邮箱客户端
  # 封装邮箱创建和邮件获取的完整流程，自动管理邮箱信息和认证令牌
  class TempEmailClient
    def initialize
      @email_info = nil
    end

    # 创建临时邮箱并缓存邮箱信息
    # @param options [GenerateEmailOptions, nil]
    # @return [EmailInfo, nil]
    def generate(options = nil)
      @email_info = Client.generate_email(options)
    end

    # 获取当前邮箱的邮件列表，必须先调用 generate
    # @param options [GetEmailsOptions, nil]
    # @return [GetEmailsResult]
    def get_emails(options = nil)
      if @email_info.nil?
        Telemetry.report(operation: "get_emails", channel: "", success: false, attempt_count: 0,
                         channels_tried: 0, error: "No email generated. Call generate() first")
        raise "No email generated. Call generate() first"
      end
      Client.get_emails(@email_info, options)
    end

    # 获取当前缓存的邮箱信息
    # @return [EmailInfo, nil]
    attr_reader :email_info
  end
end
