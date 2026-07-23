# frozen_string_literal: true

require "uri"

module TempmailSdk
  module Providers
    # openinbox.io 渠道实现
    #
    # 流程：POST /api/inbox 创建邮箱，返回 id + email
    #       GET /api/emails/inbox/{id}?page=1&limit=50 获取邮件列表
    #       GET /api/emails/{messageId} 获取邮件详情
    # token 存储 inbox id
    module Openinbox
      CHANNEL = "openinbox"
      BASE_URL = "https://api.openinbox.io/api"

      HEADERS = {
        "Accept" => "application/json, text/plain, */*",
        "Origin" => "https://openinbox.io",
        "Referer" => "https://openinbox.io/",
        "User-Agent" => "Mozilla/5.0"
      }.freeze

      module_function

      # 发起 JSON 请求
      # @param method [String] "GET" 或 "POST"
      # @param url [String]
      # @return [Object] 解析后的 JSON
      def json_request(method, url)
        hdrs = HEADERS.dup
        hdrs["Content-Type"] = "application/json" if method == "POST"
        resp = method == "POST" ? Http.post(url, headers: hdrs, timeout: 15) : Http.get(url, headers: hdrs, timeout: 15)
        resp.raise_for_status
        resp.json
      end

      # 将邮件 raw map 展平为标准字段
      # @param raw [Hash]
      # @param recipient [String]
      # @return [Hash]
      def flatten(raw, recipient)
        {
          "id" => raw["id"],
          "from" => raw["from"],
          "to" => recipient,
          "subject" => raw["subject"],
          "text" => raw["textBody"],
          "html" => raw["htmlBody"],
          "receivedAt" => raw["receivedAt"],
          "isRead" => raw["isRead"],
          "attachments" => []
        }
      end

      # 创建 openinbox.io 临时邮箱
      # @return [EmailInfo]
      def generate_email
        data = json_request("POST", BASE_URL + "/inbox")
        id = data["id"].to_s.strip
        email = data["email"].to_s.strip
        raise "openinbox: 无效的邮箱响应" if id.empty? || email.empty? || !email.include?("@")

        EmailInfo.new(channel: CHANNEL, email: email, token: id)
      end

      # 获取 openinbox.io 邮件列表
      # @param inbox_id [String] inbox id（token）
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(inbox_id, email)
        inbox_id = inbox_id.to_s.strip
        raise "openinbox: empty inbox id" if inbox_id.empty?

        url = "#{BASE_URL}/emails/inbox/#{URI.encode_uri_component(inbox_id)}?page=1&limit=50"
        data = json_request("GET", url)
        rows = data.is_a?(Hash) ? Array(data["emails"]) : []

        rows.filter_map do |row|
          next unless row.is_a?(Hash)

          raw = row
          msg_id = row["id"].to_s.strip
          unless msg_id.empty?
            detail = fetch_detail(msg_id)
            raw = detail if detail.is_a?(Hash)
          end
          Normalize.normalize_email(flatten(raw, email), email)
        end
      end

      # 获取单封邮件详情
      # @param message_id [String]
      # @return [Hash, nil]
      def fetch_detail(message_id)
        json_request("GET", "#{BASE_URL}/emails/#{URI.encode_uri_component(message_id)}")
      rescue StandardError
        nil
      end
    end
  end
end
