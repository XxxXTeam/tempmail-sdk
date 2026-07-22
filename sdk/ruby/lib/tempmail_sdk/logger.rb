# frozen_string_literal: true

require "logger"

module TempmailSdk
  # SDK 日志模块
  # 默认静默不输出，用户可通过 set_log_level 启用
  module Log
    LOG_SILENT = 100          # 关闭所有日志
    LOG_ERROR = Logger::ERROR
    LOG_WARN = Logger::WARN
    LOG_INFO = Logger::INFO
    LOG_DEBUG = Logger::DEBUG

    @logger = Logger.new($stderr)
    @logger.level = LOG_SILENT
    @logger.formatter = proc { |severity, _time, _progname, msg| "#{severity} #{msg}\n" }

    class << self
      # 设置日志级别，默认 LOG_SILENT（不输出）
      # @param level [Integer]
      def set_log_level(level)
        @logger.level = level
      end

      # 设置自定义 logger 实例
      # @param logger [Logger]
      def set_logger(logger)
        @logger = logger
      end

      # @return [Logger] 当前 SDK 使用的 logger
      def logger
        @logger
      end
    end
  end
end
