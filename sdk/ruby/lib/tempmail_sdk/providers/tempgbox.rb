# frozen_string_literal: true

require "json"

module TempmailSdk
  module Providers
    # tempgbox.net 渠道实现
    #
    # 流程：POST /api/proxy?route=generate 创建邮箱（JSON body，X-Device-ID 头）
    #       POST /api/proxy?route=inbox 获取邮件列表
    # 响应体为 HTML 页面，实际数据编码在 data-x 属性（base64 JSON）
    # token 存储 device_id
    module Tempgbox
      CHANNEL = "tempgbox"
      API_URL = "https://tempgbox.net/api/proxy"

      module_function

      # 生成随机 64 位十六进制 device id
      # @return [String]
      def random_device_id
        require "securerandom"
        SecureRandom.hex(32)
      end

      # 生成随机 IP 地址
      # @return [String]
      def random_ip
        "#{rand(1..254)}.#{rand(1..254)}.#{rand(1..254)}.#{rand(1..254)}"
      end

      # 构造请求头
      # @param device_id [String]
      # @return [Hash]
      def build_headers(device_id)
        ip = random_ip
        {
          "Accept" => "text/html,application/json",
          "Content-Type" => "application/json",
          "Origin" => "https://tempgbox.net",
          "Referer" => "https://tempgbox.net/",
          "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                          "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
          "X-Device-ID" => device_id,
          "X-Forwarded-For" => ip,
          "X-Real-IP" => ip,
          "X-Originating-IP" => ip
        }
      end

      # 从响应 HTML 中解码 data-x 属性（base64 JSON）
      # @param body [String]
      # @return [Hash]
      def decode_payload(body)
        require "base64"
        m = body.match(/data-x=["']([^"']+)["']/)
        raise "tempgbox: 响应中缺少 data-x 属性" unless m

        raw = Base64.strict_decode64(m[1])
        JSON.parse(raw)
      end

      # 发起 POST 请求并解码响应
      # @param route [String]
      # @param device_id [String]
      # @param body [Hash]
      # @return [Hash]
      def post_route(route, device_id, body)
        resp = Http.post(
          "#{API_URL}?route=#{route}",
          headers: build_headers(device_id),
          body: JSON.generate(body),
          timeout: 15
        )
        resp.raise_for_status
        decode_payload(resp.body)
      end

      # 创建 tempgbox.net 临时邮箱
      # @return [EmailInfo]
      def generate_email
        device_id = random_device_id
        payload = post_route("generate", device_id, "variant" => "googlemail")
        alias_data = payload["alias"]
        raise "tempgbox: 响应缺少 alias" unless alias_data.is_a?(Hash)

        email = alias_data["email"].to_s.strip
        email = alias_data["alias"].to_s.strip if email.empty?
        raise "tempgbox: 响应缺少 email" if email.empty?

        EmailInfo.new(channel: CHANNEL, email: email, token: device_id)
      end

      # 获取 tempgbox.net 邮件列表
      # @param device_id [String] token
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(device_id, email)
        raise "tempgbox: missing device id" if device_id.to_s.strip.empty?

        payload = post_route("inbox", device_id, "email" => email)
        messages = Array(payload["messages"])

        messages.filter_map do |raw|
          next unless raw.is_a?(Hash)

          raw["from"] ||= raw["sender"]
          raw["text"] ||= raw["body_text"]
          raw["html"] ||= raw["body_html"]
          raw["date"] ||= raw["received_at"]
          raw["messageId"] ||= raw["message_id"]
          raw["attachments"] ||= raw["attachments_info"]
          Normalize.normalize_email(raw.merge("to" => email), email)
        end
      end
    end
  end
end
