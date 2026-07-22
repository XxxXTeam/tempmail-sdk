# frozen_string_literal: true

require "json"
require "net/http"
require "uri"

module TempmailSdk
  # 匿名用量遥测
  # 默认开启，可通过配置或环境变量关闭；批量异步上报，失败静默忽略
  module Telemetry
    DEFAULT_URL = "https://sdk-1.openel.top/v1/event"
    MAX_BATCH = 32
    FLUSH_SEC = 2.0
    EMAIL_RE = /[^\s@]{1,64}@[^\s@]{1,255}/.freeze
    # 主机信息在进程内不变，预取为常量避免每次 flush 重复读取 RbConfig
    HOST_OS = (RbConfig::CONFIG["host_os"] || "unknown").freeze
    HOST_CPU = (RbConfig::CONFIG["host_cpu"] || "unknown").freeze

    @queue = []
    @lock = Mutex.new
    @flush_thread = nil

    class << self
      # 上报一条遥测事件
      # rubocop:disable Metrics/ParameterLists
      def report(operation:, channel:, success:, attempt_count:, channels_tried:, error:)
        return unless telemetry_on?

        ensure_periodic

        ev = {
          "operation" => operation,
          "channel" => channel,
          "success" => success,
          "attempt_count" => attempt_count,
          "ts_ms" => (Time.now.to_f * 1000).to_i
        }
        ev["channels_tried"] = channels_tried if channels_tried.positive?
        err = sanitize(error)
        ev["error"] = err unless err.empty?

        n = 0
        @lock.synchronize do
          @queue << ev
          n = @queue.size
        end

        flush_batch if n >= MAX_BATCH
      end
      # rubocop:enable Metrics/ParameterLists

      # @return [String] SDK 版本号
      def sdk_version
        VERSION
      end

      private

      # 脱敏：邮箱替换为占位符，截断到 400 字符
      def sanitize(msg)
        return "" if msg.nil? || msg.empty?

        msg.gsub(EMAIL_RE, "[redacted]")[0, 400]
      end

      def resolve_url
        u = (Config.get_config.telemetry_url || "").strip
        u.empty? ? DEFAULT_URL : u
      end

      def telemetry_on?
        v = Config.get_config.telemetry_enabled
        v.nil? ? true : !!v
      end

      # 启动周期性刷新线程（守护线程，进程退出即结束）
      def ensure_periodic
        @lock.synchronize do
          return if @flush_thread&.alive?

          @flush_thread = Thread.new do
            loop do
              sleep(FLUSH_SEC)
              flush_batch
            end
          end
          @flush_thread.abort_on_exception = false
        end
      end

      # 刷新队列，异步 POST 上报
      def flush_batch
        events = nil
        @lock.synchronize do
          return if @queue.empty?

          events = @queue.dup
          @queue.clear
        end

        unless telemetry_on?
          return
        end

        batch = {
          "schema_version" => 2,
          "sdk_language" => "ruby",
          "sdk_version" => VERSION,
          "os" => HOST_OS,
          "arch" => HOST_CPU,
          "events" => events
        }
        body = JSON.generate(batch)
        Thread.new { post_body(resolve_url, body) }
      end

      # 实际发送（失败静默）
      def post_body(url, body)
        uri = URI.parse(url)
        http = Net::HTTP.new(uri.host, uri.port)
        http.use_ssl = (uri.scheme == "https")
        http.open_timeout = 8
        http.read_timeout = 8
        req = Net::HTTP::Post.new(uri.request_uri)
        req["Content-Type"] = "application/json; charset=utf-8"
        req["User-Agent"] = "tempmail-sdk-ruby/#{VERSION}"
        req.body = body
        http.request(req)
      rescue StandardError
        nil
      end
    end
  end
end
