# frozen_string_literal: true

module TempmailSdk
  # 创建临时邮箱后返回的邮箱信息
  # Token 等认证信息由 SDK 内部维护，不对外暴露（token 通过私有读取器访问）
  class EmailInfo
    # @return [String] 创建该邮箱所使用的渠道
    attr_reader :channel
    # @return [String] 临时邮箱地址
    attr_accessor :email
    # @return [Integer, nil] 邮箱过期时间（毫秒时间戳）
    attr_accessor :expires_at
    # @return [Object, nil] 邮箱创建时间
    attr_accessor :created_at

    # @param channel [String] 渠道标识
    # @param email [String] 邮箱地址
    # @param token [String, nil] 认证令牌，SDK 内部维护
    # @param expires_at [Integer, nil] 过期时间（毫秒时间戳）
    # @param created_at [Object, nil] 创建时间
    def initialize(channel:, email:, token: nil, expires_at: nil, created_at: nil)
      @channel = channel
      @email = email
      @token = token
      @expires_at = expires_at
      @created_at = created_at
    end

    # 允许在同一 provider 派生多个渠道别名时覆写渠道标识
    # @param value [String] 新的渠道标识
    attr_writer :channel

    # SDK 内部使用的令牌读取器（对外语义上不暴露，仅内部 dispatch 使用）
    # @return [String, nil]
    def token
      @token
    end

    def to_s
      "#<EmailInfo channel=#{@channel.inspect} email=#{@email.inspect} " \
        "expires_at=#{@expires_at.inspect} created_at=#{@created_at.inspect}>"
    end
    alias inspect to_s
  end

  # 邮件附件信息
  class EmailAttachment
    attr_accessor :filename, :size, :content_type, :url

    def initialize(filename: "", size: nil, content_type: nil, url: nil)
      @filename = filename
      @size = size
      @content_type = content_type
      @url = url
    end

    def to_h
      { filename: @filename, size: @size, content_type: @content_type, url: @url }
    end
  end

  # 标准化的邮件对象
  class Email
    attr_accessor :id, :from_addr, :to, :subject, :text, :html, :date, :is_read, :attachments

    # rubocop:disable Metrics/ParameterLists
    def initialize(id: "", from_addr: "", to: "", subject: "", text: "", html: "",
                   date: "", is_read: false, attachments: nil)
      @id = id
      @from_addr = from_addr
      @to = to
      @subject = subject
      @text = text
      @html = html
      @date = date
      @is_read = is_read
      @attachments = attachments || []
    end
    # rubocop:enable Metrics/ParameterLists

    def to_h
      {
        id: @id, from_addr: @from_addr, to: @to, subject: @subject,
        text: @text, html: @html, date: @date, is_read: @is_read,
        attachments: @attachments.map { |a| a.respond_to?(:to_h) ? a.to_h : a }
      }
    end
  end

  # 获取邮件列表的结果
  class GetEmailsResult
    attr_accessor :channel, :email, :emails, :success

    def initialize(channel: "", email: "", emails: nil, success: true)
      @channel = channel
      @email = email
      @emails = emails || []
      @success = success
    end
  end

  # 重试配置
  class RetryConfig
    attr_accessor :max_retries, :initial_delay, :max_delay, :timeout

    # @param max_retries [Integer] 最大重试次数（不含首次请求），默认 2
    # @param initial_delay [Float] 初始重试延迟（秒），默认 1.0
    # @param max_delay [Float] 最大重试延迟（秒），默认 5.0
    # @param timeout [Float] 请求超时（秒），默认 15.0
    def initialize(max_retries: 2, initial_delay: 1.0, max_delay: 5.0, timeout: 15.0)
      @max_retries = max_retries
      @initial_delay = initial_delay
      @max_delay = max_delay
      @timeout = timeout
    end
  end

  # 创建临时邮箱的选项
  class GenerateEmailOptions
    attr_accessor :channel, :duration, :domain, :retry_config,
                  :max_channels_tried, :total_timeout, :suffix, :domains

    # rubocop:disable Metrics/ParameterLists
    def initialize(channel: nil, duration: 30, domain: nil, retry_config: nil,
                   max_channels_tried: 20, total_timeout: 60.0, suffix: nil, domains: nil)
      @channel = channel                     # 指定渠道，不指定则随机选择
      @duration = duration                   # tempmail 渠道的有效期（分钟）
      @domain = domain                       # 部分渠道指定接入域名
      @retry_config = retry_config           # 重试配置
      @max_channels_tried = max_channels_tried # 最大尝试渠道数
      @total_timeout = total_timeout         # 整体超时时间（秒）
      @suffix = suffix                       # 邮箱后缀筛选
      @domains = domains                     # 多个目标域名
    end
    # rubocop:enable Metrics/ParameterLists
  end

  # 获取邮件的选项（Channel/Email/Token 由 SDK 从 EmailInfo 自动获取）
  class GetEmailsOptions
    attr_accessor :retry_config

    def initialize(retry_config: nil)
      @retry_config = retry_config
    end
  end

  # 渠道信息
  class ChannelInfo
    attr_accessor :channel, :name, :website

    def initialize(channel: "", name: "", website: "")
      @channel = channel
      @name = name
      @website = website
    end

    def to_h
      { channel: @channel, name: @name, website: @website }
    end
  end

  # provider 尚未实现真实逻辑时抛出的错误（渠道仍可正常枚举）
  class NotImplementedProviderError < StandardError; end
end
