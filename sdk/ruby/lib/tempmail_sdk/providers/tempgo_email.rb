# frozen_string_literal: true

require "uri"

module TempmailSdk
  module Providers
    # tempgo-email 渠道 — https://tempgo.email
    # POST /api/generate 创建邮箱，GET /api/inbox 获取邮件
    module TempgoEmail
      CHANNEL = "tempgo-email"
      BASE_URL = "https://tempgo.email"

      HEADERS = {
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
        "Accept" => "application/json"
      }.freeze

      module_function

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        resp = Http.post("#{BASE_URL}/api/generate", headers: HEADERS, timeout: 15)
        resp.raise_for_status
        body = resp.json
        raise "tempgo-email: 响应格式无效" unless body.is_a?(Hash)

        address = body["email"] || ""
        mailbox_id = body["mailbox_id"] || ""
        raise "tempgo-email: 缺少邮箱地址" if address.empty?
        raise "tempgo-email: 缺少 mailbox_id" if mailbox_id.empty?

        EmailInfo.new(channel: CHANNEL, email: address, token: mailbox_id)
      end

      # 获取邮件列表
      # @param token [String] mailbox_id
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        raise "tempgo-email: token 不能为空" if token.nil? || token.empty?

        address = (email || "").strip
        raise "tempgo-email: 缺少邮箱地址" if address.empty?

        resp = Http.get("#{BASE_URL}/api/inbox?email=#{URI.encode_www_form_component(address)}&mailbox_id=#{URI.encode_www_form_component(token)}",
                        headers: HEADERS, timeout: 15)
        return [] if resp.status_code == 404
        resp.raise_for_status

        body = resp.json
        return [] unless body.is_a?(Hash)

        messages = body["messages"]
        return [] unless messages.is_a?(Array) && !messages.empty?

        messages.filter_map do |msg|
          next unless msg.is_a?(Hash)

          raw = {
            "id" => msg["id"] || "",
            "from" => msg["from"] || msg["sender"] || "",
            "to" => address,
            "subject" => msg["subject"] || "",
            "text" => msg["text"] || msg["body_plain"] || "",
            "html" => msg["html"] || msg["body_html"] || "",
            "date" => (msg["date"] || msg["received_at"] || "").to_s
          }
          Normalize.normalize_email(raw, address)
        end
      end
    end
  end
end
