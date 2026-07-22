# frozen_string_literal: true

module TempmailSdk
  # 通用重试工具
  # 提供请求重试、超时控制、指数退避等错误恢复机制
  module Retry
    # with_retry_with_attempts 的返回值封装
    class AttemptsResult
      attr_reader :ok, :attempts, :value, :error

      def initialize(ok:, attempts:, value: nil, error: nil)
        @ok = ok
        @attempts = attempts
        @value = value
        @error = error
      end

      def ok?
        @ok
      end
    end

    # 网络级别错误关键字
    NETWORK_KEYWORDS = [
      "connection", "timeout", "timed out", "dns", "eof",
      "broken pipe", "network is unreachable", "refused", "reset"
    ].freeze

    module_function

    # 判断错误是否应该重试
    # 触发重试：网络错误、429 限流、HTTP 4xx/5xx；仅参数校验类错误不重试
    # @param error [Exception]
    # @return [Boolean]
    def should_retry?(error)
      msg = error.message.to_s.downcase
      return true if NETWORK_KEYWORDS.any? { |kw| msg.include?(kw) }
      return true if msg.include?("429") || msg.include?("too many requests") || msg.include?("rate limit")
      return true if msg.include?(": 4") || msg.include?(": 5")

      # Net::HTTP 底层网络异常类型
      case error
      when Timeout::Error, Errno::ECONNREFUSED, Errno::ECONNRESET,
           SocketError, EOFError, IOError
        true
      else
        false
      end
    end

    # 带重试的操作执行器（不抛异常版本）
    # @param config [RetryConfig, nil]
    # @yield 要执行的操作
    # @return [AttemptsResult]
    def with_retry_with_attempts(config = nil)
      cfg = config || RetryConfig.new
      logger = Log.logger
      last_error = nil

      (0..cfg.max_retries).each do |attempt|
        attempts = attempt + 1
        begin
          result = yield
          logger.info("第 #{attempt + 1} 次尝试成功") if attempt.positive?
          return AttemptsResult.new(ok: true, attempts: attempts, value: result)
        rescue StandardError => e
          last_error = e
          if attempt >= cfg.max_retries || !should_retry?(e)
            return AttemptsResult.new(ok: false, attempts: attempts, error: e)
          end

          delay = [cfg.initial_delay * (2**attempt), cfg.max_delay].min
          logger.warn("请求失败 (#{e.message})，#{format('%.1f', delay)}s 后第 #{attempt + 2} 次重试...")
          sleep(delay)
        end
      end

      AttemptsResult.new(ok: false, attempts: cfg.max_retries + 1, error: last_error)
    end

    # 带重试的操作执行器（失败抛异常版本）
    # @param config [RetryConfig, nil]
    # @yield 要执行的操作
    def with_retry(config = nil, &block)
      r = with_retry_with_attempts(config, &block)
      return r.value if r.ok?

      raise r.error
    end
  end
end
