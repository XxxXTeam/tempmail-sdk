# frozen_string_literal: true

module TempmailSdk
  module Providers
    # TempMailC 渠道实现
    # API: https://tempmailc.com/api/v1
    module Tempmailc
      CHANNEL = "tempmailc"
      API_BASE = "https://tempmailc.com/api/v1"
      HEADERS = { "Accept" => "application/json" }.freeze

      module_function

      # 扁平化列表消息（无正文时使用）
      def flatten_list_message(raw, recipient)
        {
          "id" => raw["id"],
          "from" => raw["from"],
          "to" => recipient,
          "subject" => raw["subject"] || "",
          "timestamp" => raw["ts"],
          "read" => raw["read"]
        }
      end

      # 获取单封邮件详情
      def fetch_message(email, message_id)
        resp = Http.get(
          "#{API_BASE}/message?email=#{URI.encode_www_form_component(email)}&msg_id=#{URI.encode_www_form_component(message_id)}",
          headers: HEADERS, timeout: 15
        )
        return nil unless resp.ok?

        data = resp.json
        return nil unless data.is_a?(Hash) && data["ok"] != false

        data
      rescue StandardError
        nil
      end

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        resp = Http.get("#{API_BASE}/new", headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        email = data.is_a?(Hash) ? data["email"].to_s.strip : ""
        raise "tempmailc: 无效的响应" unless data.is_a?(Hash) && data["ok"] && !email.empty? && email.include?("@")

        EmailInfo.new(channel: CHANNEL, email: email)
      end

      # 获取邮件列表
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        address = email.to_s.strip
        raise "tempmailc: 邮箱地址为空" if address.empty?

        resp = Http.get(
          "#{API_BASE}/inbox?email=#{URI.encode_www_form_component(address)}",
          headers: HEADERS, timeout: 15
        )
        resp.raise_for_status
        data = resp.json
        raise "tempmailc: inbox 响应失败" unless data.is_a?(Hash) && data["ok"]

        rows = data["messages"]
        return [] unless rows.is_a?(Array)

        rows.select { |item| item.is_a?(Hash) }.map do |item|
          mid = item["id"].to_s.strip
          detail = mid.empty? ? nil : fetch_message(address, mid)
          Normalize.normalize_email(detail || flatten_list_message(item, address), address)
        end
      end
    end
  end
end
