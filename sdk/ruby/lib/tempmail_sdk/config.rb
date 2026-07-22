# frozen_string_literal: true

module TempmailSdk
  # SDK 全局配置
  # 包含代理、超时、SSL 等设置，作用于所有 HTTP 请求
  #
  # 支持环境变量自动读取（优先级低于代码设置）：
  #   TEMPMAIL_PROXY    - 代理 URL
  #   TEMPMAIL_TIMEOUT  - 超时秒数
  #   TEMPMAIL_INSECURE - 设为 "1" 或 "true" 跳过 SSL 验证
  #   DROPMAIL_AUTH_TOKEN / DROPMAIL_API_TOKEN - DropMail af_ 令牌（可选）
  #   DROPMAIL_NO_AUTO_TOKEN - 禁止自动拉取/续期
  #   DROPMAIL_RENEW_LIFETIME - renew 的 lifetime，默认 1d
  #   APIHZ_ID / APIHZ_KEY - apihz（接口盒子）调用凭据（可选）
  #   TEMPMAIL_TELEMETRY_ENABLED - true/false，默认 true；false/0/no 关闭匿名上报
  #   TEMPMAIL_TELEMETRY_URL - 自定义上报端点
  class SDKConfig
    attr_accessor :proxy, :timeout, :insecure, :headers,
                  :dropmail_auth_token, :dropmail_disable_auto_token, :dropmail_renew_lifetime,
                  :apihz_id, :apihz_key, :telemetry_enabled, :telemetry_url

    # rubocop:disable Metrics/ParameterLists
    def initialize(proxy: nil, timeout: 15, insecure: false, headers: nil,
                   dropmail_auth_token: nil, dropmail_disable_auto_token: false,
                   dropmail_renew_lifetime: nil, apihz_id: nil, apihz_key: nil,
                   telemetry_enabled: nil, telemetry_url: nil)
      @proxy = proxy                                       # 代理 URL，支持 http/https/socks5
      @timeout = timeout                                   # 全局默认超时秒数
      @insecure = insecure                                 # 跳过 SSL 证书验证（调试用）
      @headers = headers                                   # 自定义请求头，合并到每个请求
      @dropmail_auth_token = dropmail_auth_token           # DropMail af_ 令牌；空则自动申请
      @dropmail_disable_auto_token = dropmail_disable_auto_token # true 时不自动 generate/renew
      @dropmail_renew_lifetime = dropmail_renew_lifetime   # renew lifetime，默认 1d
      @apihz_id = apihz_id                                 # apihz 调用 id；空则用公共账号
      @apihz_key = apihz_key                               # apihz 调用 key；空则用公共账号
      @telemetry_enabled = telemetry_enabled               # nil 默认开启；false 关闭
      @telemetry_url = telemetry_url                       # 非空覆盖默认上报 URL
    end
    # rubocop:enable Metrics/ParameterLists
  end

  # 配置模块：全局配置读写与版本号（供 HTTP 客户端缓存失效判断）
  module Config
    module_function

    # 从环境变量读取默认配置
    # @return [SDKConfig]
    def load_env_config
      timeout_str = ENV.fetch("TEMPMAIL_TIMEOUT", nil)
      timeout = timeout_str&.match?(/\A\d+\z/) ? timeout_str.to_i : 15
      insecure = %w[1 true True TRUE].include?(ENV.fetch("TEMPMAIL_INSECURE", ""))
      dm_tok = (ENV["DROPMAIL_AUTH_TOKEN"] || ENV["DROPMAIL_API_TOKEN"] || "").strip
      dm_tok = nil if dm_tok.empty?
      dm_no = %w[1 true yes].include?((ENV["DROPMAIL_NO_AUTO_TOKEN"] || "").strip.downcase)
      dm_renew = (ENV["DROPMAIL_RENEW_LIFETIME"] || "").strip
      dm_renew = nil if dm_renew.empty?

      te_raw = (ENV["TEMPMAIL_TELEMETRY_ENABLED"] || "").strip.downcase
      telemetry_enabled = nil
      telemetry_enabled = false if %w[false 0 no].include?(te_raw)
      telemetry_enabled = true if %w[true 1 yes].include?(te_raw)

      tu = (ENV["TEMPMAIL_TELEMETRY_URL"] || "").strip
      tu = nil if tu.empty?
      apihz_id = (ENV["APIHZ_ID"] || "").strip
      apihz_id = nil if apihz_id.empty?
      apihz_key = (ENV["APIHZ_KEY"] || "").strip
      apihz_key = nil if apihz_key.empty?
      proxy = ENV.fetch("TEMPMAIL_PROXY", nil)
      proxy = nil if proxy && proxy.empty?

      SDKConfig.new(
        proxy: proxy, timeout: timeout, insecure: insecure,
        dropmail_auth_token: dm_tok, dropmail_disable_auto_token: dm_no,
        dropmail_renew_lifetime: dm_renew, telemetry_enabled: telemetry_enabled,
        telemetry_url: tu, apihz_id: apihz_id, apihz_key: apihz_key
      )
    end

    @global_config = load_env_config
    @config_version = 0

    class << self
      # 设置 SDK 全局配置
      # 设置后自动使已缓存的 HTTP 客户端失效，下次请求按新配置重建
      # @param config [SDKConfig, nil] 传入配置对象；为 nil 时用关键字参数构造
      def set_config(config = nil, **kwargs)
        @global_config = config || SDKConfig.new(**kwargs)
        @config_version += 1
        nil
      end

      # @return [SDKConfig] 当前全局配置
      def get_config
        @global_config
      end

      # @return [Integer] 当前配置版本号
      def config_version
        @config_version
      end
    end
  end
end
