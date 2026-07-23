# frozen_string_literal: true

require "uri"
require "json"

module TempmailSdk
  module Providers
    # tempmail.cn 渠道实现（HTTP API 模式）
    #
    # 流程：WebSocket 创建邮箱（request mailbox 事件），获取 shortID
    #       GET /api/mails/{email} 获取邮件列表（HTTP API）
    # Ruby 端使用 HTTP API 拉取邮件，不实现 WebSocket 推送
    # token 不需要（无状态 HTTP API）
    module TempmailCn
      CHANNEL = "tempmail-cn"
      DEFAULT_HOST = "tempmail.cn"

      module_function

      # 规范化 host（去除协议前缀、路径、@ 符号等）
      # @param domain [String, nil]
      # @return [String]
      def normalize_host(domain)
        return DEFAULT_HOST if domain.nil? || domain.to_s.strip.empty?

        host = domain.to_s.strip
        if host.start_with?("http://", "https://")
          begin
            uri = URI.parse(host)
            host = uri.host.to_s unless uri.host.to_s.empty?
          rescue URI::InvalidURIError
            nil
          end
        elsif host.include?("/")
          host = host.split("/").first.to_s
        end
        host = host.split("@").last.to_s if host.include?("@")
        host = host.strip.gsub(/\A\.+|\.+\z/, "")
        host.empty? ? DEFAULT_HOST : host
      end

      # 生成随机本地部分（小写字母 + 数字）
      # @param length [Integer]
      # @return [String]
      def random_local(length = 10)
        chars = ("a".."z").to_a + ("0".."9").to_a
        Array.new(length) { chars.sample }.join
      end

      # 创建 tempmail.cn 临时邮箱
      # @param domain [String, nil]
      # @return [EmailInfo]
      def generate_email(domain = nil)
        host = normalize_host(domain)
        # 直接生成随机邮箱（HTTP 模式，不走 WebSocket）
        email = "#{random_local}@#{host}"
        EmailInfo.new(channel: CHANNEL, email: email)
      end

      # 获取 tempmail.cn 邮件列表
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        email = email.to_s.strip
        raise "tempmail-cn: 邮箱地址为空" if email.empty?

        at_idx = email.index("@")
        host = at_idx ? email[(at_idx + 1)..] : DEFAULT_HOST
        host = normalize_host(host)

        api_url = "https://#{host}/api/mails/#{URI.encode_uri_component(email)}"
        hdrs = {
          "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                          "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
          "Accept" => "application/json",
          "Accept-Language" => "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
          "Cache-Control" => "no-cache",
          "DNT" => "1",
          "Pragma" => "no-cache",
          "Referer" => "https://#{host}/"
        }

        resp = Http.get(api_url, headers: hdrs, timeout: 15)
        resp.raise_for_status

        data = resp.json
        mails = data.is_a?(Hash) ? Array(data["mails"]) : []

        mails.filter_map do |raw|
          next unless raw.is_a?(Hash)

          id = raw["id"].to_s
          id = raw["_id"].to_s if id.empty?
          flat = {
            "id" => id,
            "from" => raw["from"].to_s,
            "to" => email,
            "subject" => raw["subject"].to_s,
            "text" => raw["text"].to_s,
            "html" => raw["html"].to_s,
            "date" => raw["date"].to_s,
            "isRead" => false,
            "attachments" => raw["attachments"]
          }
          flat["text"] = raw["body"].to_s if flat["text"].empty?
          Normalize.normalize_email(flat, email)
        end
      end
    end
  end
end
